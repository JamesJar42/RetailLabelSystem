#include "../include/Menu.h"


int Menu::getUserData()
{
    int value;

    std::cin >> value;

    return value;
}

void Menu::display()
{

    std::cout << title << std::endl;
    for (size_t i = 0; i < title.length(); i++)
    {
        std::cout << "+";
    }

    std::cout << std::endl;


    for (size_t opt = 1; opt <= items.size(); opt++)
    {
        std::cout << opt << ". " << items[opt - 1] << std::endl;
    }
    std::cout << std::endl;

}

Menu::Menu(std::string title, std::vector<std::string> data)
    :title(title), items(data) {}

Menu::~Menu()
{

}
