#include<string>
#include<vector>
#include<iostream>

#pragma once

class Menu
{
	std::vector<std::string> items;
	std::string title;


public:
	Menu(std::string title, std::vector<std::string> data);

	~Menu();

	int getUserData();

	void display();
};

