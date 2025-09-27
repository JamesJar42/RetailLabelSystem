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
#include <windows.h>
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

public:
	shop dtb;

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
	void flagAll();
	void unflagAll();
	void changePrice();
	void deletePages();
	void print();
	void viewLabels();
	labelSystem(const std::string &config);
	~labelSystem();
};
