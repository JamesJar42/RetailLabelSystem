#include "../include\/labelSystem.h"
#include "Menu.h"

/*
* 
* This .cpp file uses the labelSystem class and starts a QT application. 
* It manages starts the application and displays a menu for user choice.
* 
*/

labelSystem ls("resources/Config.txt");

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

	SetConsoleOutputCP(CP_UTF8);

	ls.process();

	vector<string> items = {"Add Products", "Remove Product Via Barcode", "Edit Product Via Barcode", 
	"Search by Barcode", "Search By Name", "Flag/Unflag Products", "Unflag All Products", "Change Price", "Print Pages", "Quit"};

	Menu Main("Label System", items);

	do
	{

		ls.save();

		cout << endl;

		Main.display();

		std::cout << "Please Enter Your Choice (Number): ";

		switch (Main.getUserData())
		{
		case 1:
			ls.addProduct();
			break;
		case 2:
			if (!ls.dtb.listProduct().empty())
			{
				ls.removeProducts();
			}
			else
			{
				std::cout << std::endl << "There Are No Products In The Database" << std::endl;
			}
			break;
		case 3:
			if (!ls.dtb.listProduct().empty())
			{
				ls.editByBarcode();
			}
			else
			{
				std::cout << std::endl << "There Are No Products In The Database" << std::endl;
			}
			break;
		case 4:
			if (!ls.dtb.listProduct().empty())
			{
				ls.searchByBarcode();
			}
			else
			{
				std::cout << std::endl << "There Are No Products In The Database" << std::endl;
			}
			break;
		case 5:
			if (!ls.dtb.listProduct().empty())
			{
				ls.searchByName();
			}
			else
			{
				std::cout << std::endl << "There Are No Products In The Database" << std::endl;
			}
			break;
		case 6:
			ls.flagProducts();
			break;
		case 7:
			ls.unflagAll();
			break;
		case 8:
			ls.changePrice();
			break;
		case 9:
			ls.viewLabels();
			break;
		case 10:
			exit(0);
			break;
		default:
			std::cin.clear();
			std::cin.ignore(100, '\n');
			std::cout << std::endl;
			std::cout << "Error" << std::endl << std::endl;
		}


	} while (true);

	return app.exec();
}