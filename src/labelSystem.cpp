#include "../include/labelSystem.h"
#include "../include/Menu.h"

/*
* 
* The labelSystem class makes use of the shop.h file in order to use those functions and store products.
* This class also makes use of QT and OpenCV functions in order to handle image processing.
* The class contains functions that are to be used by the run.cpp file in order to handle user processes in a menu.
* 
*/

labelSystem::labelSystem(const std::string &config)
{
	init(config);
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
}

std::vector<cv::Mat> labelSystem::loadImages(const std::vector<std::string>& filenames) {
	std::vector<cv::Mat> images;
	for (const auto& file : filenames) {
		images.push_back(cv::imread(file));
	}
	return images;
}

QImage labelSystem::cvMatToQImage(const cv::Mat& mat) {
	return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888).rgbSwapped();
}

void labelSystem::UpperCaseWords(std::string &strings)
{
	vector<std::string> editString;

	std::string newString;

	std::stringstream ss(strings);
	std::string sub;
	while (ss >> sub)
	{
		editString.push_back(sub);
	}

	for (int i = 0; i < editString.size(); i++)
	{

		if (!std::isdigit(editString[i].at(0))) 
		{
			editString[i].at(0) = std::toupper(editString[i].at(0));
		}
		newString += editString[i] + " ";
	}

	newString.pop_back();

	strings = newString;

	editString.clear();
	editString.shrink_to_fit();

	//return newString;

}

void labelSystem::printPreview(QPrinter& printer, const std::vector<QImage>& images) {

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
	std::vector<QImage> images;
	for (const cv::Mat& cvImg : cvImages) {
		images.push_back(cvMatToQImage(cvImg));
	}

	QPrinter printer;
	QPrintPreviewDialog preview(&printer);

	QObject::connect(&preview, &QPrintPreviewDialog::paintRequested, [&](QPrinter* printer) {
		printPreview(*printer, images);
		});

	preview.exec();
}

void labelSystem::process()
{

	int dtbSize;

	std::ifstream database("resources/Database.txt");

	std::cout << "Attempting to read Database..." << std::endl;
	if (database.is_open()) {
		std::cout << "Read Successful" << std::endl;
		database >> dtbSize;

		for (int i = 0; i < dtbSize; i++) {
			std::string type, Name, Barcode, Size;
			float Price = 0, PriceEach;
			bool Flag = false;

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
					database >> Flag;
				}

				if (type == "Size:") {
					database >> Size;
				}

				if (type == "End") {
					dtb.add(product(Name, Price, Size, Barcode, Flag));
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
	fout.open("resources/Database.txt");
	fout << dtb.listProduct().size() << std::endl;
	for (product& pd : dtb.listProduct())
	{
		fout << pd.toString() << std::endl;
	}
	fout.close();
}

void labelSystem::addProduct()
{
	bool error = false;

	clear();
	do {
		error = false;
		try {


			std::string description, barcode, size = "";
			float price;
			bool labelFlag = true;

			product pd(description, price = 0, size, barcode, labelFlag);

			std::cin.clear();
			std::cin.ignore(100, '\n');

			std::cout << "Please Enter Product Name (Press 'q' to Exit): ";
			std::getline(std::cin, description);
			if (description == "q")
			{
				break;
			}
			UpperCaseWords(description);
			std::cout << std::endl;

			std::cout << "Please Enter the Price: £";
			std::cin >> price;
			std::cout << std::endl;

			if (std::cin.fail())
			{
				throw "Error";
			}

			std::cin.clear();
			std::cin.ignore(100, '\n');

			std::cout << "Please Enter the Size: ";
			std::getline(std::cin, size);
			std::cout << std::endl;

			std::cout << "Please Enter the Barcode: ";
			std::cin >> barcode;
			std::cout << std::endl;
			for (product& pd : dtb.listProduct())
			{
				if (barcode == pd.getBarcode())
				{
					std::cout << "A Product Already Exists With That Barcode" << std::endl;
					throw "Error";
				}
			}

			pd.setDescription(description);
			pd.setPrice(price);
			pd.setSize(size);
			pd.setBarcode(barcode);

			dtb.add(pd);

			clear();
			std::cout << "Product Created" << std::endl << "Product Flagged" << std::endl;
		}
		catch (...)
		{
			std::cout << "Error Occurred. Please Try Again." << std::endl;
			std::cout << std::endl;
			error = true;
			std::cin.clear();
			std::cin.ignore(100, '\n');
		}


	} while (error);


}

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

void labelSystem::editSize(product& pd)
{
	clear();

	std::string size;

	std::cin.ignore(100, '\n');

	std::cout << "Enter New Product Size: ";
	std::getline(std::cin, size);

	pd.setSize(size);
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

	vector<string> items = { "Edit Name", "Edit Price", "Edit Size", "Edit Barcode", "Return" };

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
				std::cout << "Size: " << pdt.getSize() << std::endl;
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
					editSize(pdt);
					break;
				case 4:
					editBarcode(pdt);
					break;
				case 5:
					clear();
					break;
				default:
					std::cin.clear();
					std::cin.ignore(100, '\n');
					std::cout << std::endl;
					std::cout << "Error" << std::endl << std::endl;
					break;
				}

			} while (choice != 5);
		}
	}
	if (found == false)
	{
		std::cout << "Product Not Found" << std::endl;
		std::cin.ignore(100, '\n');
	}
}

void labelSystem::flagProducts()
{

	bool done;
	clear();
	do
	{

		std::string barcode;
		std::cout << "Enter Barcode or 'q' to return: ";
		std::cin >> barcode;
		std::cout << std::endl;
		bool finished = false;

		if (barcode == "q")
		{
			break;
		}

		for (product& pd : dtb.listProduct())
		{
			if (pd.getBarcode() == barcode)
			{
				finished = true;
				if (!pd.getLabelFlag())
				{
					pd.setLabelFlag(true);
					std::cout << "Product Flagged" << std::endl;
				}
				else
				{
					pd.setLabelFlag(false);
					std::cout << "Product Flag Removed" << std::endl;
				}
			}
		}
		if (finished == false)
		{
			std::cout << "Product Not Found" << std::endl;
		}
	} while (true);

	clear();
}

void labelSystem::flagAll()
{
	for (product& pds : dtb.listProduct())
	{
		pds.setLabelFlag(true);
	}
	std::cout << "All Products Flagged" << std::endl << std::endl;
}

void labelSystem::unflagAll()
{
	clear();

	for (product& pds : dtb.listProduct())
	{
		pds.setLabelFlag(false);
	}
	std::cout << "All Products Unflagged" << std::endl << std::endl;
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
	clear();

	bool labelsFound = false;


	cv::Mat img = cv::imread("resources/labelTemplate.png", cv::IMREAD_COLOR);
	cv::Mat labels(3508, 2480, CV_8UC3, cv::Scalar(255, 255, 255));
	cv::namedWindow("Output", cv::WINDOW_NORMAL);

	int x_offset = labelconfig.XO;
	int y_offset = 0;


	for (product& pd : dtb.listProduct()) {

		if (pd.getLabelFlag()) {

			labelsFound = true;

			cv::Point porg(labelconfig.PX, labelconfig.PY);
			cv::Point dorg(labelconfig.TX, labelconfig.TY);

			cv::Mat resized;
			cv::Mat temp;
			img.copyTo(temp);

			std::string p = std::to_string(pd.getPrice());
			std::string rp = p.substr(0, p.find(".") + 3);

			cv::addText(temp, "£" + rp, porg, "times", labelconfig.PS, cv::Scalar(0, 0, 255), cv::QT_FONT_NORMAL,
				cv::QT_STYLE_NORMAL,
				0);

			if (pd.getDescription().append(" ").size() + pd.getSize().size() > labelconfig.TL)
			{
				std::string str = (pd.getDescription().append(" ") + pd.getSize());
				int pos = str.find_last_of(' ', labelconfig.TL);

				std::string subStr1 = str.substr(0, pos);
				std::string subStr2 = str.substr(pos + 1, (str.size() - 1));

				cv::Point dorg1(labelconfig.STX, labelconfig.STY);

				cv::addText(temp, subStr1, dorg, "times", labelconfig.TS, cv::Scalar(12, 12, 12), cv::QT_FONT_NORMAL,
					cv::QT_STYLE_NORMAL, 0);
				cv::addText(temp, subStr2, dorg1, "times", labelconfig.TS, cv::Scalar(12, 12, 12), cv::QT_FONT_NORMAL,
					cv::QT_STYLE_NORMAL, 0);
			}
			else
			{
				cv::addText(temp, pd.getDescription() + " " + pd.getSize(), dorg, "times", labelconfig.TS, cv::Scalar(12, 12, 12), cv::QT_FONT_NORMAL,
					cv::QT_STYLE_NORMAL, 0);
			}

			cv::resize(temp, resized, cv::Size(480, 320), cv::INTER_LINEAR);

			if (x_offset + resized.cols > labels.cols) {
				y_offset += resized.rows;
				x_offset = labelconfig.XO;
			}
			if (y_offset + resized.rows > labels.rows)
			{
				labelVector.push_back(labels);
				labels = cv::Mat(3508, 2480, CV_8UC3, cv::Scalar(255, 255, 255));
				y_offset = 0;
			}

			cv::Mat roi(labels, cv::Rect(x_offset, y_offset, resized.cols, resized.rows));

			resized.copyTo(roi);

			x_offset += resized.cols;

			temp.release();
		}
	}

	if (labelsFound == false)
	{
		std::cout << "Product not found" << std::endl;
	}

	if (labelVector.size() > 0)
	{
		int count = 1;
		cv::imwrite("labels.jpg", labels);
		for (auto lbs : labelVector)
		{
			cv::imwrite("labels(" + std::to_string(count) + ").jpg", lbs);
			count++;
		}
	}
	else
	{
		cv::imwrite("labels.jpg", labels);
	}

	if (labelVector.size() > 0 || labelsFound)
	{
		print();
	}
	else
	{
		std::cout << "There Were No Pages To Print" << std::endl;
	}

	cv::destroyAllWindows();

	bool error = false;

	do {
		error = false;
		char choice, del;
		std::cout << "Remove Label Flags (Y or N)" << std::endl;
		std::cin >> choice;
		if (tolower(choice) == 'y') {
			for (product& pd : dtb.listProduct()) {
				if (pd.getLabelFlag()) {
					pd.setLabelFlag(false);
				}
			}
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
