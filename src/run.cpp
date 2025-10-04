#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../include/labelSystem.h"
#include "Menu.h"
#include "MainWindow.h"
#include <QApplication>
#include <QFile>
#include <QString>
#include <QSettings>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

/*
* 
* This .cpp file uses the labelSystem class and starts a QT application. 
* It manages starts the application and displays a menu for user choice.
* 
*/

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
		// Handle the CTRL-C signal.
	case CTRL_C_EVENT:
		printf("Ctrl-C event\n\n");
		Beep(750, 300);
		return TRUE;

	default:
		return FALSE;
	}
}

int main(int argc, char* argv[])
{
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	QApplication app(argc, argv);

	// Ensure compiled resources from icons.qrc are initialized (makes :/icons/* available)
	Q_INIT_RESOURCE(icons);

	// Setup organization/app for QSettings
	QCoreApplication::setOrganizationName("RetailLabelCo");
	QCoreApplication::setApplicationName("RetailLabeler");

	SetConsoleOutputCP(CP_UTF8);

	// Ensure the working directory is the directory containing the executable
	// so relative resource paths like "resources/..." resolve correctly.
	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(NULL, exePath, MAX_PATH);
	std::filesystem::path exeDir = std::filesystem::path(exePath).remove_filename();
	std::filesystem::current_path(exeDir);

	// Initialize labelSystem
	labelSystem ls("resources/Config.txt");

	// Select stylesheet according to the saved theme preference.
	// Possible values: "light", "dark", "file" (editable resources/style.qss).
	QSettings settings;
	QString theme = settings.value("theme", "light").toString();
	// Allow temporary override from command-line for testing: --theme=dark|light|file
	for (int i = 1; i < argc; ++i) {
		QString arg = QString::fromLocal8Bit(argv[i]);
		if (arg.startsWith("--theme=", Qt::CaseInsensitive)) {
			QString val = arg.section('=', 1, 1).trimmed();
			if (val == "dark" || val == "light" || val == "file") {
				qDebug() << "Command-line theme override:" << val;
				theme = val;
				// record a transient override so MainWindow reloads honor it
				app.setProperty("themeOverride", val);
			}
		}
	}
	QString qssPath;
	QString chosen;
	// candidate list declared early
	QStringList candidates;
	bool qssPathSet = false;
	if (theme == "file") {
		// If the user selected a custom qss path earlier, try that absolute path first
		QString custom = settings.value("custom_qss", "").toString();
		if (!custom.isEmpty() && QFile::exists(custom)) {
			qssPath = custom;
			qssPathSet = true;
		} else {
			chosen = "style.qss";
		}
	}
	else if (theme == "dark") chosen = "style_dark.qss";
	else chosen = "style_light.qss";

	// If a custom absolute path was set above, use it. Otherwise probe candidate locations.
	if (!qssPathSet) {
		candidates << (QStringLiteral("resources/") + chosen);
		candidates << (QStringLiteral("../resources/") + chosen);
		candidates << (QStringLiteral("../../resources/") + chosen);
		QString found;
		for (const QString &c : candidates) {
			if (QFile::exists(c)) { found = c; break; }
		}
		if (!found.isEmpty()) qssPath = found;
		else if (QFile::exists("resources/style.qss")) qssPath = "resources/style.qss";
		else qssPath = candidates.first();
	}
	if (QFile::exists(qssPath)) {
		QFile qssFile(qssPath);
		if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
			QString style = QString::fromUtf8(qssFile.readAll());
			app.setStyleSheet(style);
			qssFile.close();
			qDebug() << "Loaded stylesheet:" << qssPath;
		}
	} else {
		qDebug() << "No stylesheet found at" << qssPath;
	}

	// Pass labelSystem to MainWindow
	MainWindow mainWindow(&ls);
	// Diagnostic: list embedded icon resources (helpful when qrc embedding fails)
	{
		// ...existing code...
	}
	mainWindow.show();

	return app.exec();
}