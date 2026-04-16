#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
using namespace std;

class Event
{
public:
    int id, seats, price;
    string name, date, venue;

    Event() {}

    Event(int i, string n, string d, string v, int p, int s)
    {
        id = i;
        name = n;
        date = d;
        venue = v;
        price = p;
        seats = s;
    }
};

class BookingSystem
{
    vector<Event> events;

public:
    BookingSystem()
    {
        loadEvents();
    }

    void loadEvents()
    {
        ifstream file("D:/OOP Project/data/events.txt");

        string line;

        while (getline(file, line))
        {
            Event e;
            string temp;

            stringstream ss(line);

            getline(ss, temp, '|');
            e.id = stoi(temp);
            getline(ss, e.name, '|');
            getline(ss, e.date, '|');
            getline(ss, e.venue, '|');
            getline(ss, temp, '|');
            e.price = stoi(temp);
            getline(ss, temp, '|');
            e.seats = stoi(temp);

            events.push_back(e);
        }

        file.close();
    }

    void saveEvents()
    {
        ofstream file("D:/OOP Project/data/events.txt");

        for (auto e : events)
        {
            file << e.id << "|"
                 << e.name << "|"
                 << e.date << "|"
                 << e.venue << "|"
                 << e.price << "|"
                 << e.seats << endl;
        }

        file.close();
    }

    void showEvents()
    {
        for (auto e : events)
        {
            cout << e.id << "|"
                 << e.name << "|"
                 << e.date << "|"
                 << e.venue << "|"
                 << e.price << "|"
                 << e.seats << endl;
        }
    }

    void book(int id, string user, int seats)
    {
        for (auto &e : events)
        {
            if (e.id == id && e.seats >= seats)
            {

                e.seats -= seats;
                saveEvents();

                // ✅ ABSOLUTE PATH (FIX)
                ofstream file("D:/OOP Project/data/tickets.txt", ios::app);

                if (!file)
                {
                    cout << "FILE_ERROR";
                    return;
                }

                file << user << " "
                     << id << " "
                     << seats << endl;

                file.close();

                cout << "BOOKED";
                return;
            }
        }

        cout << "FAILED";
    }
    void showHistory()
    {
        ifstream file("D:/OOP Project/data/tickets.txt");

        string line;
        while (getline(file, line))
        {
            cout << line << endl;
        }

        file.close();
    }
    void addEvent(string name, string date, string venue, int price, int seats)
    {
        int newId = events.empty() ? 1 : events.back().id + 1;

        ofstream file("D:/OOP Project/data/events.txt", ios::app);

        file << newId << "|"
             << name << "|"
             << date << "|"
             << venue << "|"
             << price << "|"
             << seats << endl;

        file.close();

        cout << "EVENT_ADDED";
    }

    void cancel(string user, int id, int seats)
    {
        vector<string> lines;
        ifstream file("D:/OOP Project/data/tickets.txt");

        string u;
        int eid, s;
        bool found = false;

        while (file >> u >> eid >> s)
        {
            if (u == user && eid == id && s == seats && !found)
            {
                found = true;

                for (auto &e : events)
                {
                    if (e.id == id)
                        e.seats += seats;
                }
            }
            else
            {
                lines.push_back(u + " " + to_string(eid) + " " + to_string(s));
            }
        }

        file.close();

        ofstream out("D:/OOP Project/data/tickets.txt");
        for (auto l : lines)
            out << l << endl;

        saveEvents();

        if (found)
            cout << "CANCELLED";
        else
            cout << "NOT_FOUND";
    }
};

int main(int argc, char *argv[])
{

    BookingSystem system;

    string action = argv[1];

    if (action == "events")
    {
        system.showEvents();
    }
    else if (action == "book")
    {
        int id = stoi(argv[2]);
        string user = argv[3];
        int seats = stoi(argv[4]);
        system.book(id, user, seats);
    }
    else if (action == "cancel")
    {
        string user = argv[2];
        int id = stoi(argv[3]);
        int seats = stoi(argv[4]);
        system.cancel(user, id, seats);
    }
    else if (action == "add")
    {
        string name = argv[2];
        string date = argv[3];
        string venue = argv[4];
        int price = stoi(argv[5]);
        int seats = stoi(argv[6]);

        system.addEvent(name, date, venue, price, seats);
    }

    return 0;
}