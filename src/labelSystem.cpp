#include "../include/labelSystem.h"
#include "../include/Menu.h"
#include "../include/PrintDialog.h"
#include <thread>
#include <chrono>
#include <QSettings>
#include <QFile>
#include <QPrintDialog>
#include <QDateTime>
#include <QPainter>
#include <QFont>
#include <QPen>
#include <QDebug>

namespace {
void appendCrashTrace(const QString &msg) {
	QFile f("resources/CrashTrace.log");
	if (f.open(QIODevice::Append | QIODevice::Text)) {
		QString line = QString("[%1] %2\n")
			.arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"), msg);
		f.write(line.toUtf8());
		f.close();
	}
}

labelConfig defaultLabelConfig()
{
	return {24, 44, 140, 15, 80, 90, 350, 15, 140, 40};
}

bool deletePagesWithRetryImpl(int initialDelayMs, int maxAttempts, int attemptIntervalMs)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(initialDelayMs));

	auto tryDeleteFile = [&](const std::string &name) -> bool {
		for (int attempt = 0; attempt < maxAttempts; ++attempt) {
			std::error_code ec;
			const bool removed = std::filesystem::remove(name, ec);
			if (removed) return true;
			if (!std::filesystem::exists(name)) return true;

			std::ofstream log("resources/DeletionLog.txt", std::ios::app);
			if (log.is_open()) {
				log << "Attempt " << attempt + 1 << " to delete " << name << " failed; will retry.\n";
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(attemptIntervalMs));
		}
		return false;
	};

	bool ok = true;
	if (!tryDeleteFile("labels.jpg")) ok = false;

	for (int i = 1; ; ++i) {
		const std::string name = "labels(" + std::to_string(i) + ").jpg";
		if (!std::filesystem::exists(name)) break;
		if (!tryDeleteFile(name)) ok = false;
	}

	std::ofstream log("resources/DeletionLog.txt", std::ios::app);
	if (log.is_open()) {
		log << "deletePagesWithRetry completed: " << (ok ? "SUCCESS" : "FAILED") << "\n";
	}

	return ok;
}
}

/*
* 
* The labelSystem class makes use of the shop.h file in order to use those functions and store products.
* This class also makes use of QT and OpenCV functions in order to handle image processing.
* The class contains functions that are to be used by the run.cpp file in order to handle user processes in a menu.
* 
*/

labelSystem::labelSystem(const std::string &config)
{
	labelconfig = defaultLabelConfig();
	init(config);
}

labelSystem::labelSystem()
{
	labelconfig = defaultLabelConfig();
}

labelSystem::~labelSystem()
{
}

void labelSystem::init(const std::string& path)
{
	std::ifstream config(path);
	std::string type;

	if (config.is_open())
	{
		while (config >> type)
		{
			if (type == "labels")
			{
				config >> labelconfig.TL >> labelconfig.TS >> labelconfig.PS >> labelconfig.TX >> labelconfig.TY >> labelconfig.PX 
					>> labelconfig.PY >> labelconfig.STX >> labelconfig.STY >> labelconfig.XO;
			}
		}
	}

	config.close();

	// Apply runtime/user overrides from settings (if present) so print layout
	// does not require manual edits to Config.txt.
	QSettings settings;
	labelconfig.TL = settings.value("printLayout/TL", labelconfig.TL).toInt();
	labelconfig.TS = settings.value("printLayout/TS", labelconfig.TS).toInt();
	labelconfig.PS = settings.value("printLayout/PS", labelconfig.PS).toInt();
	labelconfig.TX = settings.value("printLayout/TX", labelconfig.TX).toInt();
	labelconfig.TY = settings.value("printLayout/TY", labelconfig.TY).toInt();
	labelconfig.PX = settings.value("printLayout/PX", labelconfig.PX).toInt();
	labelconfig.PY = settings.value("printLayout/PY", labelconfig.PY).toInt();
	labelconfig.STX = settings.value("printLayout/STX", labelconfig.STX).toInt();
	labelconfig.STY = settings.value("printLayout/STY", labelconfig.STY).toInt();
	labelconfig.XO = settings.value("printLayout/XO", labelconfig.XO).toInt();
}

// Resolve a possibly-relative resource path to an absolute path based on the
// application's directory so resources are found even when launched from a
// shortcut whose working directory differs.
std::string labelSystem::resourcePath(const std::string &relPath) const
{
	std::filesystem::path p(relPath);
	if (p.is_absolute()) return relPath;

	QString appDir = QCoreApplication::applicationDirPath();
	if (!appDir.isEmpty()) {
		std::filesystem::path abs = std::filesystem::path(appDir.toStdString()) / relPath;
		return abs.make_preferred().string();
	}

	return (std::filesystem::current_path() / relPath).make_preferred().string();
}

std::vector<cv::Mat> labelSystem::loadImages(const std::vector<std::string>& filenames) {
	std::vector<cv::Mat> images;
	for (const auto& file : filenames) {
		images.push_back(cv::imread(resourcePath(file)));
	}
	return images;
}

QImage labelSystem::cvMatToQImage(const cv::Mat& mat) {
	return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888).rgbSwapped();
}

std::vector<cv::Mat> labelSystem::buildLabelPages(const labelConfig &cfg, bool &labelsFound) {
	appendCrashTrace("buildLabelPages: start");
	labelsFound = false;
	std::vector<cv::Mat> pages;

	QSettings settings;
	QString customLogo = settings.value("custom_logo_path").toString();
	const bool useQtAddText = settings.value("print/useQtAddText", true).toBool();
	std::string logoPathToUse;
	if (!customLogo.isEmpty() && QFile::exists(customLogo)) {
		logoPathToUse = customLogo.toStdString();
	}

	cv::Mat img;
	if (!logoPathToUse.empty()) {
		img = cv::imread(logoPathToUse, cv::IMREAD_COLOR);
	}
	if (img.empty()) {
		img = cv::Mat(320, 480, CV_8UC3, cv::Scalar(255, 255, 255));
	}

	cv::Mat labels(3508, 2480, CV_8UC3, cv::Scalar(255, 255, 255));
	int x_offset = cfg.XO;
	int y_offset = 0;

	for (product& pd : dtb.listProduct()) {
		int qty = pd.getLabelQuantity();
		if (qty <= 0) {
			continue;
		}

		for (int i = 0; i < qty; ++i) {
			labelsFound = true;

			cv::Point porg(cfg.PX, cfg.PY);
			cv::Point dorg(cfg.TX, cfg.TY);

			cv::Mat resized;
			cv::Mat temp;
			img.copyTo(temp);

			auto drawWithQPainter = [&](const std::string &text, const cv::Point &pt, int pixelSize, const QColor &color) {
				QImage qimg(temp.data, temp.cols, temp.rows, static_cast<int>(temp.step), QImage::Format_BGR888);
				QPainter painter(&qimg);
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setRenderHint(QPainter::TextAntialiasing, true);
				QFont font("Times New Roman");
				font.setPixelSize(std::max(1, pixelSize));
				painter.setFont(font);
				painter.setPen(QPen(color));
				painter.drawText(QPoint(pt.x, pt.y), QString::fromStdString(text));
				painter.end();
			};

			std::string p = std::to_string(pd.getPrice());
			std::string rp = p.substr(0, p.find(".") + 3);
			std::string priceText = "£" + rp;

			auto clampedScale = [](int s, double minS, double maxS, double divisor) {
				double scale = static_cast<double>(s) / divisor;
				return std::max(minS, std::min(maxS, scale));
			};

			const double priceScale = clampedScale(cfg.PS, 0.5, 6.0, 60.0);
			const double textScale = clampedScale(cfg.TS, 0.4, 4.0, 45.0);
			const int priceThickness = std::max(1, static_cast<int>(priceScale));
			const int textThickness = std::max(1, static_cast<int>(textScale));

			if (useQtAddText) {
				drawWithQPainter(priceText, porg, cfg.PS, QColor(255, 0, 0));
			} else {
				cv::putText(temp, priceText, porg, cv::FONT_HERSHEY_SIMPLEX, priceScale,
					cv::Scalar(0, 0, 255), priceThickness, cv::LINE_AA);
			}

			if (pd.getDescription().append(" ").size() > cfg.TL)
			{
				std::string str = pd.getDescription().append(" ");
				int pos = str.find_last_of(' ', cfg.TL);

				std::string subStr1 = str.substr(0, pos);
				std::string subStr2 = str.substr(pos + 1, (str.size() - 1));

				cv::Point dorg1(cfg.STX, cfg.STY);

				if (useQtAddText) {
					drawWithQPainter(subStr1, dorg, cfg.TS, QColor(12, 12, 12));
					drawWithQPainter(subStr2, dorg1, cfg.TS, QColor(12, 12, 12));
				} else {
					cv::putText(temp, subStr1, dorg, cv::FONT_HERSHEY_SIMPLEX, textScale,
						cv::Scalar(12, 12, 12), textThickness, cv::LINE_AA);
					cv::putText(temp, subStr2, dorg1, cv::FONT_HERSHEY_SIMPLEX, textScale,
						cv::Scalar(12, 12, 12), textThickness, cv::LINE_AA);
				}
			}
			else
			{
				if (useQtAddText) {
					drawWithQPainter(pd.getDescription(), dorg, cfg.TS, QColor(12, 12, 12));
				} else {
					cv::putText(temp, pd.getDescription(), dorg, cv::FONT_HERSHEY_SIMPLEX, textScale,
						cv::Scalar(12, 12, 12), textThickness, cv::LINE_AA);
				}
			}

			cv::resize(temp, resized, cv::Size(480, 320), cv::INTER_LINEAR);

			if (x_offset + resized.cols > labels.cols) {
				y_offset += resized.rows;
				x_offset = cfg.XO;
			}
			if (y_offset + resized.rows > labels.rows)
			{
				pages.push_back(labels.clone());
				labels = cv::Mat(3508, 2480, CV_8UC3, cv::Scalar(255, 255, 255));
				y_offset = 0;
				x_offset = cfg.XO;
			}

			cv::Mat roi(labels, cv::Rect(x_offset, y_offset, resized.cols, resized.rows));
			resized.copyTo(roi);
			x_offset += resized.cols;
		}
	}

	if (labelsFound) {
		pages.push_back(labels);
	}
	appendCrashTrace(QString("buildLabelPages: end pages=%1 labelsFound=%2").arg(static_cast<int>(pages.size())).arg(labelsFound));

	return pages;
}

std::vector<QImage> labelSystem::buildPrintImages(const labelConfig &cfg) {
	bool labelsFound = false;
	std::vector<cv::Mat> pages = buildLabelPages(cfg, labelsFound);
	std::vector<QImage> images;
	for (const cv::Mat &page : pages) {
		images.push_back(cvMatToQImage(page));
	}
	return images;
}

void labelSystem::UpperCaseWords(std::string &strings)
{
	if (strings.empty()) {
		return;
	}

	std::vector<std::string> editString;

	std::string newString;

	std::stringstream ss(strings);
	std::string sub;
	while (ss >> sub)
	{
		editString.push_back(sub);
	}

	for (int i = 0; i < editString.size(); i++)
	{
		if (!editString[i].empty() && !std::isdigit(static_cast<unsigned char>(editString[i].at(0)))) 
		{
			editString[i].at(0) = static_cast<char>(std::toupper(static_cast<unsigned char>(editString[i].at(0))));
		}
		newString += editString[i] + " ";
	}

	if (!newString.empty()) {
		newString.pop_back();
	}

	strings = newString;

	editString.clear();
	editString.shrink_to_fit();

	//return newString;

}

void labelSystem::printPreview(QPrinter& printer, const std::vector<QImage>& images) 
{

	QPainter painter(&printer);
	QRect rect = painter.viewport();

	int count = 0;
	for (const QImage& image : images) {
		count++;
		QSize size = image.size();
		size.scale(rect.size(), Qt::KeepAspectRatio);

		painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
		painter.setWindow(image.rect());
		painter.drawImage(0, 0, image);

		if (count < images.size())
		{
			printer.newPage();
		}  // Move to next page for multiple images
	}
}

void labelSystem::printImages(const std::vector<cv::Mat>& cvImages) {
	appendCrashTrace(QString("printImages: start cvImages=%1").arg(static_cast<int>(cvImages.size())));
	std::vector<QImage> images;
	for (const cv::Mat& cvImg : cvImages) {
		images.push_back(cvMatToQImage(cvImg));
	}
	appendCrashTrace(QString("printImages: converted images=%1").arg(static_cast<int>(images.size())));

	QSettings settings;
	const bool useNativePrintDialog = settings.value("print/useNativeDialog", false).toBool();
	if (useNativePrintDialog) {
		appendCrashTrace("printImages: native dialog create");
		QPrinter printer(QPrinter::ScreenResolution);
		QPrintDialog nativeDialog(&printer);
		appendCrashTrace("printImages: native dialog exec");
		if (nativeDialog.exec() == QDialog::Accepted) {
			appendCrashTrace("printImages: native dialog accepted");
			printPreview(printer, images);
		}
		appendCrashTrace("printImages: native dialog done");

		std::thread([]() {
			appendCrashTrace("printImages: deletePagesWithRetry begin");
			deletePagesWithRetryImpl(2000, 12, 500);
			appendCrashTrace("printImages: deletePagesWithRetry end");
		}).detach();
		appendCrashTrace("printImages: native path return");
		return;
	}

	PrintLayoutConfig initialLayout {
		labelconfig.TL,
		labelconfig.TS,
		labelconfig.PS,
		labelconfig.TX,
		labelconfig.TY,
		labelconfig.PX,
		labelconfig.PY,
		labelconfig.STX,
		labelconfig.STY,
		labelconfig.XO
	};

	QString configSummary = QString(
		"From Config.txt\n"
		"TL=%1  TS=%2  PS=%3\n"
		"TX=%4  TY=%5\n"
		"PX=%6  PY=%7\n"
		"STX=%8  STY=%9\n"
		"XO=%10"
	)
		.arg(labelconfig.TL)
		.arg(labelconfig.TS)
		.arg(labelconfig.PS)
		.arg(labelconfig.TX)
		.arg(labelconfig.TY)
		.arg(labelconfig.PX)
		.arg(labelconfig.PY)
		.arg(labelconfig.STX)
		.arg(labelconfig.STY)
		.arg(labelconfig.XO);

	PrintDialog dialog(
		images,
		configSummary,
		initialLayout,
		[this](const PrintLayoutConfig &cfg) {
			labelConfig internalCfg{cfg.TL, cfg.TS, cfg.PS, cfg.TX, cfg.TY, cfg.PX, cfg.PY, cfg.STX, cfg.STY, cfg.XO};
			this->setLabelConfig(internalCfg);
			return this->buildPrintImages(internalCfg);
		}
	);
	appendCrashTrace("printImages: custom dialog exec");

	dialog.exec();
	appendCrashTrace("printImages: custom dialog done");

	// Start deletion asynchronously after dialog returns so GUI responsiveness is preserved.
	std::thread([]() {
		deletePagesWithRetryImpl(2000, 12, 500);
	}).detach();
}

void labelSystem::process()
{
    // Try to load from CSV first
    CSVMapping map;
    map.hasHeader = true;
    map.barcodeCol = 0;
    map.nameCol = 1;
    map.priceCol = 2;
    map.originalPriceCol = 4;
    map.labelQuantityCol = 5;

    if (dtb.loadFromCSV(resourcePath("resources/Database.csv"), map)) {
        std::cout << "Database loaded from CSV." << std::endl;
        return;
    }
    
    // Fallback or legacy (can be removed if strictly moving to CSV)
    std::cout << "Database.csv not found, checking legacy..." << std::endl;

	int dtbSize;

	std::ifstream database(resourcePath("resources/Database.txt"));

	std::cout << "Attempting to read Database..." << std::endl;
	if (database.is_open()) {
		std::cout << "Read Successful" << std::endl;
		database >> dtbSize;

		for (int i = 0; i < dtbSize; i++) {
			std::string type, Name, Barcode, Size;
			float Price = 0, PriceEach;
			int Quantity = 0;
            bool tempFlag = false;

			while (database >> type)
			{
				if (type == "Name:") {
					getline(database, Name, ',');
					Name.erase(0, Name.find_first_not_of(' '));
					Name.erase(Name.find_last_not_of(' ') + 1);
				}

				if (type == "Price:") {
					database >> Price;
				}

				if (type == "PriceEach:") {
					database >> PriceEach;
				}

				if (type == "Barcode:") {
					database >> Barcode;
				}

				if (type == "Flag:") {
					database >> tempFlag;
                    Quantity = tempFlag ? 1 : 0;
				}
                
                if (type == "Quantity:") {
                    database >> Quantity;
                }

				if (type == "Size:") {
                    std::string tempSize;
					database >> tempSize;
				}

				if (type == "End") {
					dtb.add(product(Name, Price, Barcode, Quantity));
				}
			}

		}

		database.close();
	}
	else
	{
		std::cout << "Couldn't Open File" << std::endl;
	}



}

void labelSystem::clear()
{
	std::cout << std::string(40, '\n');
}

void labelSystem::listProducts()
{
	int count = 1;
	for (product pd : dtb.listProduct())
	{
		std::cout << std::to_string(count) << ". " << pd.toString();
		std::cout << std::endl;
		count++;
	}

}

void labelSystem::save()
{
	std::ofstream fout;
	fout.open(resourcePath("resources/Database.txt"));
	fout << dtb.listProduct().size() << std::endl;
	for (product& pd : dtb.listProduct())
	{
		fout << pd.toString() << std::endl;
	}
	fout.close();
}

// void labelSystem::addProduct()
// {
// 	bool error = false;

// 	clear();
// 	do {
// 		error = false;
// 		try {


// 			// By default, newly added products are flagged (quantity 1)
// 			int labelQuantity = 1;
// 			std::string description, barcode;
// 			float price = 0;

// 			product pd(description, price, barcode, labelQuantity);

// 			std::cin.clear();
// 			std::cin.ignore(100, '\n');

// 			std::cout << "Please Enter Product Name (Press 'q' to Exit): ";
// 			std::getline(std::cin, description);
// 			if (description == "q")
// 			{
// 				break;
// 			}
// 			UpperCaseWords(description);
// 			std::cout << std::endl;

// 			std::cout << "Please Enter the Price: £";
// 			std::cin >> price;
// 			std::cout << std::endl;

// 			if (std::cin.fail())
// 			{
// 				throw "Error";
// 			}

// 			std::cin.clear();
// 			std::cin.ignore(100, '\n');

// 			std::cout << "Please Enter the Barcode: ";
// 			std::cin >> barcode;
// 			std::cout << std::endl;
// 			for (product& pd : dtb.listProduct())
// 			{
// 				if (barcode == pd.getBarcode())
// 				{
// 					std::cout << "A Product Already Exists With That Barcode" << std::endl;
// 					throw "Error";
// 				}
// 			}

// 			pd.setDescription(description);
// 			pd.setPrice(price);
// 			pd.setBarcode(barcode);

// 			dtb.add(pd);

// 			clear();
// 			std::cout << "Product Created" << std::endl << "Product Flagged" << std::endl;
// 		}
// 		catch (...)
// 		{
// 			std::cout << "Error Occurred. Please Try Again." << std::endl;
// 			std::cout << std::endl;
// 			error = true;
// 			std::cin.clear();
// 			std::cin.ignore(100, '\n');
// 		}


// 	} while (error);


// }

void labelSystem::removeProducts()
{
	clear();

	bool finished = false;
	std::string pBarcode;
	std::cout << "Please Enter A Barcode for a Product to Remove: " << std::endl;
	std::cin >> pBarcode;

	for (product& pdt : dtb.listProduct())
	{
		if (pdt.getBarcode() == pBarcode)
		{
			finished = true;
			dtb.listProduct().erase(remove(dtb.listProduct().begin(),
				dtb.listProduct().end(), pdt),
				dtb.listProduct().end());
			clear();
			std::cout << "Product Removed" << std::endl;
		}
	}

	if (finished == false)
	{
		std::cout << "Product Not Found" << std::endl;
	}

}

void labelSystem::editName(product& pd)
{
	clear();

	std::cin.ignore(100, '\n');

	std::string name;
	std::cout << "Enter New Product Name: ";
	std::getline(std::cin, name);
	UpperCaseWords(name);

	pd.setDescription(name);
}

void labelSystem::editPrice(product& pd)
{
	bool error = false;

	clear();

	do {
		error = false;
		float price;
		std::cout << "Enter New Product Price: ";
		std::cin >> price;

		if (std::cin.fail())
		{
			error = true;
			std::cout << "Error Occurred. Please Try Again." << std::endl;
			std::cin.clear();
			std::cin.ignore(100, '\n');

		}

		pd.setPrice(price);
	} while (error);
}



void labelSystem::editBarcode(product& pd)
{
	clear();

	std::string barcode;
	std::cout << "Enter New Product Barcode: ";
	std::cin >> barcode;
	std::cin.ignore(100, '\n');

	pd.setBarcode(barcode);
}

void labelSystem::searchByBarcode()
{

	clear();

	std::string barcode;
	std::cout << "Enter Barcode: ";
	std::cin >> barcode;
	std::cout << std::endl;
	bool finished = false;

	for (product& pd : dtb.listProduct())
	{
		if (pd.getBarcode() == barcode)
		{
			finished = true;
			clear();
			std::cout << pd.toString();
			std::cout << std::endl;
			break;
		}
	}
	if (finished == false)
	{
		clear();
		std::cout << "Product Not Found" << std::endl;
	}
}

void labelSystem::searchByName()
{
	shop pds;
	std::string name;

	clear();

	std::cout << "Enter Name:";
	std::cin >> name;

	for (product& pd : dtb.listProduct())
	{
		if (pd.getDescription().find(name) != std::string::npos)
		{
			clear();
			std::cout << pd.toString();
			std::cout << std::endl << std::endl;
			pds.add(pd);
		}
	}
	if (pds.listProduct().empty())
	{
		clear();
		std::cout << "No Products Found" << std::endl;
	}
}

void labelSystem::editByBarcode()
{
	clear();

	int choice = 0;
	std::string pBarcode;
	std::cout << "Enter Barcode: ";
	std::cin >> pBarcode;
	std::cout << std::endl;

	std::vector<std::string> items = { "Edit Name", "Edit Price", "Edit Barcode", "Return" };

	Menu edit("Edit", items);

	bool found = false;

	for (product& pdt : dtb.listProduct())
	{
		if (pdt.getBarcode() == pBarcode)
		{
			found = true;
			do {
				clear();

				save();

				std::cout << std::endl;

				std::cout << "Products Current Information:" << std::endl;
				std::cout << "Name: " << pdt.getDescription() << std::endl;
				std::cout << "Price: " << pdt.getPrice() << std::endl;
				std::cout << "Barcode: " << pdt.getBarcode() << std::endl;

				std::cout << std::endl;

				edit.display();

				std::cout << "Please Enter Your Choice (Number): ";

				choice = edit.getUserData();

				switch (choice)
				{
				case 1:
					editName(pdt);
					break;
				case 2:
					editPrice(pdt);
					break;
				case 3:
					editBarcode(pdt);
					break;
				case 4:
					clear();
					break;
				default:
					std::cin.clear();
					std::cin.ignore(100, '\n');
					std::cout << std::endl;
					std::cout << "Error" << std::endl << std::endl;
					break;
				}

			} while (choice != 4);
		}
	}
	if (found == false)
	{
		std::cout << "Product Not Found" << std::endl;
		std::cin.ignore(100, '\n');
	}
}

void labelSystem::queueProducts()
{

	bool done;
	clear();
	do
	{
		std::string barcode;
		std::cout << "Enter Barcode to add to queue or 'q' to return: ";
		std::cin >> barcode;
		std::cout << std::endl;
		bool match = false;

		if (barcode == "q")
		{
			break;
		}

		for (product& pd : dtb.listProduct())
		{
			if (pd.getBarcode() == barcode)
			{
				match = true;
				pd.setLabelQuantity(pd.getLabelQuantity() + 1);
				std::cout << "Added to queue. New Quantity: " << pd.getLabelQuantity() << std::endl;
			}
		}
		if (!match)
		{
			std::cout << "Product Not Found" << std::endl;
		}
	} while (true);

	clear();
}

// Batch add products to queue by barcode. Returns number of matched products updated.
int labelSystem::queueProducts(const std::vector<std::string> &barcodes, int quantity)
{
	int matched = 0;
	if (barcodes.empty()) return matched;

	for (const std::string &bc : barcodes) {
		for (product &pd : dtb.listProduct()) {
			if (pd.getBarcode() == bc) {
				pd.setLabelQuantity(pd.getLabelQuantity() + quantity);
				++matched;
				break; // move to next barcode once matched
			}
		}
	}

	return matched;
}

void labelSystem::addAllToQueue(int quantity)
{
	for (product& pds : dtb.listProduct())
	{
		pds.setLabelQuantity(pds.getLabelQuantity() + quantity);
	}
	std::cout << "All Products Added to Queue" << std::endl << std::endl;
}

void labelSystem::clearQueue()
{
	if (!QCoreApplication::instance()) clear(); // Only clear console if not in GUI mode

	for (product& pds : dtb.listProduct())
	{
		pds.setLabelQuantity(0);
	}
	std::cout << "Queue Cleared" << std::endl << std::endl;
}

void labelSystem::changePrice()
{

	clear();

	int choice = 0;
	std::string pBarcode;
	std::cout << "Enter Barcode: ";
	std::cin >> pBarcode;
	std::cout << std::endl;

	bool found = false;

	for (product& pdt : dtb.listProduct())
	{
		if (pdt.getBarcode() == pBarcode)
		{
			found = true;


			bool error = false;

			clear();

			do {
				error = false;
				float price;
				std::cout << pdt.getDescription() << std::endl;
				std::cout << "Current Price: £" << pdt.getPrice() << std::endl;
				std::cout << "Enter New Product Price: £";
				std::cin >> price;

				if (std::cin.fail())
				{
					error = true;
					std::cout << "Error Occurred. Please Try Again." << std::endl;
					std::cin.clear();
					std::cin.ignore(100, '\n');

				}

				pdt.setPrice(price);
			} while (error);

		}
	}
	if (found == false)
	{
		std::cout << "Product Not Found" << std::endl;
		std::cin.ignore(100, '\n');
	}

}

void labelSystem::deletePages()
{
	if (std::filesystem::remove("labels.jpg"))
	{
		std::cout << "File Deleted Successfully" << std::endl;
	}
	else
	{
		std::cout << "File Cannot Be Deleted" << std::endl;
	}

	if (labelVector.size() > 0)
	{
		for (int i = 1; i <= labelVector.size(); i++)
		{
			if (std::filesystem::remove("labels(" + std::to_string(i) + ").jpg"))
			{
				std::cout << "File Deleted Successfully" << std::endl;
			}
			else
			{
				std::cout << "File Cannot Be Deleted" << std::endl;
			}
		}
	}
}

bool labelSystem::deletePagesWithRetry(int initialDelayMs, int maxAttempts, int attemptIntervalMs)
{
	return deletePagesWithRetryImpl(initialDelayMs, maxAttempts, attemptIntervalMs);
}

void labelSystem::clearAllFlags()
{
	for (product& pd : dtb.listProduct())
	{
		pd.setLabelQuantity(0);
	}
	std::cout << "All product flags cleared." << std::endl << std::endl;
}

void labelSystem::print()
{

	std::vector<std::string> filenames = { "labels.jpg" };

	for (int i = 1; i <= labelVector.size(); i++) {
		filenames.push_back("labels(" + std::to_string(i) + ").jpg");
	}

	std::vector<cv::Mat> cvImages = loadImages(filenames);
	printImages(cvImages);

}

void labelSystem::viewLabels()
{
	// Reload config and any dialog overrides before generating labels.
	init(resourcePath("resources/Config.txt"));

	clear();

	bool labelsFound = false;
	labelVector = buildLabelPages(labelconfig, labelsFound);

	if (!labelsFound || labelVector.empty()) {
		std::cout << "There Were No Pages To Print" << std::endl;
	} else {
		printImages(labelVector);
	}

	// After showing the print preview, ask the user whether to clear the print queue
	// and whether to delete generated pages.
	if (QCoreApplication::instance()) {
		// If running with a GUI, simply return control to the caller. The
		// MainWindow will handle asking the user via dialog and call
		// clearQueue() and deletePages() as needed.
		return;
	} else {
		bool error = false;

		do {
			error = false;
			char choice, del;
			std::cout << "Clear Print Queue? (Y or N)" << std::endl;
			std::cin >> choice;
			if (tolower(choice) == 'y') {
				clearQueue();
			}
			else if (tolower(choice) == 'n') {

			}
			else {
				std::cout << "Error Please Try Again" << std::endl;
				error = true;
				std::cin.ignore(100, '\n');
			}

			std::cout << "Delete Pages? (Y or N)" << std::endl;
			std::cin >> del;
			if (tolower(del) == 'y') {
				deletePages();
			}
			else if (tolower(del) == 'n') {

			}
			else {
				std::cout << "Error Please Try Again" << std::endl;
				error = true;
				std::cin.ignore(100, '\n');
			}
		} while (error);
	}

}

labelConfig labelSystem::getLabelConfig() const {
	return labelconfig;
}

void labelSystem::setLabelConfig(const labelConfig &cfg) {
	labelconfig = cfg;
}

bool labelSystem::saveConfig(const std::string &path)
{
	try {
		std::ofstream fout(resourcePath(path));
		if (!fout.is_open()) return false;
		// Write the config in the format expected by the reader (leading 'labels')
		fout << "labels " << labelconfig.TL << " " << labelconfig.TS << " " << labelconfig.PS << " "
			 << labelconfig.TX << " " << labelconfig.TY << " " << labelconfig.PX << " " << labelconfig.PY << " "
			 << labelconfig.STX << " " << labelconfig.STY << " " << labelconfig.XO << std::endl;
		fout.close();
		return true;
	} catch (const std::exception &e) {
		qWarning() << "Failed to save config:" << e.what();
		return false;
	}
}
