#include<string>
#include<vector>
#include<iostream>

#pragma once

using namespace std;

class Menu
{
	vector<string> items;
	string title;


public:
	Menu(string title, vector<string> data);

	~Menu();

	int getUserData();

	void display();
};

