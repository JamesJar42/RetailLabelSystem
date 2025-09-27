#include "../include\/Menu.h"


int Menu::getUserData()
{
    int value;

    cin >> value;

    return value;
}

void Menu::display()
{

    cout << title << endl;
    for (int i = 0; i < title.length(); i++)
    {
        cout << "+";
    }

    cout << endl;


    for (int opt = 1; opt <= items.size(); opt++)
    {
        cout << opt << ". " << items[opt - 1] << endl;
    }
    cout << endl;

}

Menu::Menu(string title, vector<string> data)
    :title(title), items(data) {}

Menu::~Menu()
{

}
