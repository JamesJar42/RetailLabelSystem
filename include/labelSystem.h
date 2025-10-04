#pragma once
#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <QApplication>
#include <QWidget>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QPrintDialog>
#include <QPainter>
#include <QImage>
#include <QPrintPreviewWidget>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include "product.h"
#include "shop.h"

struct labelConfig { int TL, TS, PS, TX, TY, PX, PY, STX, STY, XO; };


class labelSystem
{
	std::vector<cv::Mat> labelVector;

	labelConfig labelconfig;
	std::vector<cv::Mat> loadImages(const std::vector<std::string>& filenames);
	QImage cvMatToQImage(const cv::Mat& mat);

	void init(const std::string &path);

	// Resolve a possibly-relative resource path to an absolute path based on the
	// application's directory so resources are found even when launched from a
	// shortcut whose working directory differs.
	std::string resourcePath(const std::string &relPath) const;

public:
	shop dtb;

	labelSystem();
	void UpperCaseWords(std::string &string);
	void printPreview(QPrinter& printer, const std::vector<QImage>& images);
	void printImages(const std::vector<cv::Mat>& cvImages);
	void process();
	void clear();
	void listProducts();
	void save();
	void addProduct();
	void removeProducts();
	void editName(product& pd);
	void editPrice(product& pd);
	void editSize(product& pd);
	void editBarcode(product& pd);
	void searchByBarcode();
	void searchByName();
	void editByBarcode();
	void flagProducts();
	// Flag/unflag multiple products by barcode. Returns number of matched products updated.
	int flagProducts(const std::vector<std::string> &barcodes, bool setFlag = true);
	void flagAll();
	void unflagAll();
	void changePrice();
	void deletePages();
	// Attempt to delete generated pages after a delay with retries. Returns true on success.
	bool deletePagesWithRetry(int initialDelayMs = 2000, int maxAttempts = 10, int attemptIntervalMs = 500);
	void print();
	void viewLabels();

	// Clear label flags for all products
	void clearLabelFlags();

	// Accessors for the label configuration
	labelConfig getLabelConfig() const;
	void setLabelConfig(const labelConfig &cfg);
	// Save the current label configuration back to the config file
	bool saveConfig(const std::string &path = "resources/Config.txt");
	labelSystem(const std::string &config);
	~labelSystem();
};
