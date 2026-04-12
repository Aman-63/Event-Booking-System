#ifdef _WIN32
    #define _WIN32_WINNT 0x0601
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR   -1
    #define CLOSE_SOCKET   close
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>

using namespace std;

const string EVENTS_FILE  = "../data/events.txt";
const string TICKETS_FILE = "../data/tickets.txt";
const string FRONTEND_DIR = "../frontend/";
const int    PORT         = 3000;

// event class

class Event {
public:
    int id, seats, price;
    string name, date, venue;

    Event() : id(0), seats(0), price(0) {}
    Event(int i, string n, string d, string v, int p, int s)
        : id(i), name(n), date(d), venue(v), price(p), seats(s) {}
};

// booking class

class BookingSystem {
    vector<Event> events;

    void loadEvents() {
        events.clear();
        ifstream f(EVENTS_FILE);
        if (!f) {
            cerr << "  [ERROR] Cannot open: " << EVENTS_FILE << "\n"
                 << "  Run server.exe from inside the backend/ folder!\n";
            return;
        }
        string line;
        while (getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            if ((int)count(line.begin(), line.end(), '|') < 5) continue;
            try {
                Event e;
                string tmp;
                istringstream ss(line);
                getline(ss, tmp,     '|'); e.id    = stoi(tmp);
                getline(ss, e.name,  '|');
                getline(ss, e.date,  '|');
                getline(ss, e.venue, '|');
                getline(ss, tmp,     '|'); e.price = stoi(tmp);
                getline(ss, tmp,     '|');
                if (!tmp.empty() && tmp.back() == '\r') tmp.pop_back();
                e.seats = stoi(tmp);
                events.push_back(e);
            } catch (...) {
                cerr << "  [WARN] Skipping malformed line: " << line << "\n";
            }
        }
        cout << "  [OK] Loaded " << events.size() << " event(s)\n";
    }

    void saveEvents() {
        ofstream f(EVENTS_FILE);
        for (auto& e : events)
            f << e.id << "|" << e.name << "|" << e.date << "|"
              << e.venue << "|" << e.price << "|" << e.seats << "\n";
    }

public:
    string getEventsJSON() {
        loadEvents();
        if (events.empty()) return "[]";
        ostringstream out;
        out << "[";
        for (size_t i = 0; i < events.size(); ++i) {
            auto& e = events[i];
            if (i) out << ",";
            out << "{"
                << "\"id\":"      << e.id    << ","
                << "\"name\":\""  << e.name  << "\","
                << "\"date\":\""  << e.date  << "\","
                << "\"venue\":\"" << e.venue << "\","
                << "\"price\":"   << e.price << ","
                << "\"seats\":"   << e.seats
                << "}";
        }
        out << "]";
        return out.str();
    }

    string book(int eventId, const string& user, int seats) {
        cout << "  [BOOK] eventId=" << eventId << " user=" << user << " seats=" << seats << "\n";
        loadEvents();
        for (auto& e : events) {
            if (e.id == eventId) {
                cout << "  [BOOK] Found '" << e.name << "' seats=" << e.seats << "\n";
                if (e.seats >= seats) {
                    e.seats -= seats;
                    saveEvents();
                    ofstream f(TICKETS_FILE, ios::app);
                    if (!f) return "FILE_ERROR";
                    f << user << " " << eventId << " " << seats << "\n";
                    cout << "  [BOOKED] OK\n";
                    return "BOOKED";
                }
                cout << "  [BOOK] Not enough seats\n";
                return "FAILED";
            }
        }
        cout << "  [BOOK] Event not found\n";
        return "FAILED";
    }

    string cancel(const string& user, int eventId, int seats) {
        loadEvents();
        vector<string> keep;
        ifstream fin(TICKETS_FILE);
        if (!fin) return "NOT_FOUND";

        string u; int eid, s;
        bool found = false;
        while (fin >> u >> eid >> s) {
            if (!found && u == user && eid == eventId && s == seats) {
                found = true;
                for (auto& e : events)
                    if (e.id == eventId) e.seats += seats;
            } else {
                keep.push_back(u + " " + to_string(eid) + " " + to_string(s));
            }
        }
        fin.close();
        if (!found) return "NOT_FOUND";

        ofstream fout(TICKETS_FILE);
        for (auto& l : keep) fout << l << "\n";
        saveEvents();
        cout << "  [CANCELLED] " << user << " x" << seats << "\n";
        return "CANCELLED";
    }

    string getHistoryJSON() {
        loadEvents();
        ifstream f(TICKETS_FILE);
        if (!f) return "[]";

        string u; int eid, s;
        ostringstream out;
        out << "[";
        bool first = true;
        while (f >> u >> eid >> s) {
            if (!first) out << ",";
            first = false;
            string evName = "Unknown";
            for (auto& e : events) if (e.id == eid) evName = e.name;
            out << "{"
                << "\"type\":\"book\","
                << "\"user\":\""      << u       << "\","
                << "\"eventId\":"     << eid     << ","
                << "\"eventName\":\"" << evName  << "\","
                << "\"seats\":"       << s
                << "}";
        }
        out << "]";
        return out.str();
    }

    string addEvent(const string& name, const string& date,
                    const string& venue, int price, int seats) {
        loadEvents();
        int newId = events.empty() ? 1 : events.back().id + 1;
        ofstream f(EVENTS_FILE, ios::app);
        if (!f) return "FILE_ERROR";
        f << newId << "|" << name << "|" << date << "|"
          << venue << "|" << price << "|" << seats << "\n";
        cout << "  [ADDED] Event: " << name << "\n";
        return "EVENT_ADDED";
    }
};

// http helper

// Extract a value from flat JSON by key
string jsonGet(const string& body, const string& key) {
    string needle = "\"" + key + "\"";
    size_t pos = body.find(needle);
    if (pos == string::npos) return "";
    pos = body.find(':', pos + needle.size());
    if (pos == string::npos) return "";
    ++pos;
    while (pos < body.size() && (body[pos]==' '||body[pos]=='\t'||body[pos]=='\r'||body[pos]=='\n')) ++pos;
    if (pos >= body.size()) return "";

    if (body[pos] == '"') {
        ++pos;
        string val;
        while (pos < body.size() && body[pos] != '"') {
            if (body[pos] == '\\') ++pos;
            if (pos < body.size()) val += body[pos++];
        }
        return val;
    } else {
        size_t end = pos;
        while (end < body.size() && body[end]!=',' && body[end]!='}' &&
               body[end]!='\r' && body[end]!='\n') ++end;
        string val = body.substr(pos, end - pos);
        size_t a = val.find_first_not_of(" \t");
        size_t b = val.find_last_not_of(" \t\r\n");
        return (a == string::npos) ? "" : val.substr(a, b - a + 1);
    }
}

string readFile(const string& path) {
    ifstream f(path, ios::binary);
    if (!f) return "";
    ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

string httpResponse(int code, const string& ct, const string& body) {
    string status = code==200 ? "200 OK" : code==404 ? "404 Not Found" : "500 Error";
    ostringstream r;
    r << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: "   << ct   << "\r\n"
      << "Access-Control-Allow-Origin: *\r\n"
      << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
      << "Access-Control-Allow-Headers: Content-Type\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << body;
    return r.str();
}

string jsonResp(const string& b) { return httpResponse(200, "application/json",      b); }
string textResp(const string& b) { return httpResponse(200, "text/plain",             b); }
string htmlResp(const string& b) { return httpResponse(200, "text/html",              b); }
string cssResp (const string& b) { return httpResponse(200, "text/css",               b); }
string jsResp  (const string& b) { return httpResponse(200, "application/javascript", b); }

// for request parsing
struct HttpRequest {
    string method, path, body;
    int contentLength = 0;
};

HttpRequest parseRequest(const string& raw) {
    HttpRequest req;

    // Parse first line
    size_t lineEnd = raw.find("\r\n");
    if (lineEnd == string::npos) lineEnd = raw.find("\n");
    if (lineEnd == string::npos) return req;

    istringstream firstLine(raw.substr(0, lineEnd));
    string proto;
    firstLine >> req.method >> req.path >> proto;

    // Strip query string
    size_t q = req.path.find('?');
    if (q != string::npos) req.path = req.path.substr(0, q);

    // Split headers and body at \r\n\r\n
    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == string::npos) {
        headerEnd = raw.find("\n\n");
        if (headerEnd == string::npos) return req;
        req.body = raw.substr(headerEnd + 2);
    } else {
        req.body = raw.substr(headerEnd + 4);
    }

    // Read Content-Length
    string headers = raw.substr(0, headerEnd);
    string headersLower = headers;
    transform(headersLower.begin(), headersLower.end(), headersLower.begin(), ::tolower);
    size_t clPos = headersLower.find("content-length:");
    if (clPos != string::npos) {
        size_t vs = clPos + 15;
        while (vs < headers.size() && headers[vs] == ' ') ++vs;
        size_t ve = headers.find("\r\n", vs);
        try { req.contentLength = stoi(headers.substr(vs, ve - vs)); } catch (...) {}
    }

    // Trim body to Content-Length
    if (req.contentLength > 0 && (int)req.body.size() > req.contentLength)
        req.body = req.body.substr(0, req.contentLength);

    return req;
}

// reqyest handling
BookingSystem bs;

string handleRequest(const string& raw) {
    HttpRequest req = parseRequest(raw);

    cout << "  --> " << req.method << " " << req.path << "\n";
    if (!req.body.empty())
        cout << "  --> body: " << req.body << "\n";

    if (req.method == "OPTIONS")
        return httpResponse(200, "text/plain", "");

    
    if (req.method == "GET" && (req.path == "/" || req.path == "/index.html")) {
        string c = readFile(FRONTEND_DIR + "index.html");
        return c.empty() ? httpResponse(404, "text/plain", "index.html not found") : htmlResp(c);
    }
    if (req.method == "GET" && req.path == "/style.css") {
        string c = readFile(FRONTEND_DIR + "style.css");
        return c.empty() ? httpResponse(404, "text/plain", "style.css not found") : cssResp(c);
    }
    if (req.method == "GET" && req.path == "/script.js") {
        string c = readFile(FRONTEND_DIR + "script.js");
        return c.empty() ? httpResponse(404, "text/plain", "script.js not found") : jsResp(c);
    }

    
    if (req.method == "GET" && req.path == "/events")  return jsonResp(bs.getEventsJSON());
    if (req.method == "GET" && req.path == "/history") return jsonResp(bs.getHistoryJSON());

    if (req.method == "POST" && req.path == "/book") {
        string user = jsonGet(req.body, "user");
        string eid  = jsonGet(req.body, "eventId");
        string s    = jsonGet(req.body, "seats");
        cout << "  --> parsed: user='" << user << "' eventId='" << eid << "' seats='" << s << "'\n";
        int eventId = eid.empty() ? 0 : stoi(eid);
        int seats   = s.empty()   ? 0 : stoi(s);
        return textResp(bs.book(eventId, user, seats));
    }

    if (req.method == "POST" && req.path == "/cancel") {
        string user = jsonGet(req.body, "user");
        string eid  = jsonGet(req.body, "eventId");
        string s    = jsonGet(req.body, "seats");
        int eventId = eid.empty() ? 0 : stoi(eid);
        int seats   = s.empty()   ? 0 : stoi(s);
        return textResp(bs.cancel(user, eventId, seats));
    }

    if (req.method == "POST" && req.path == "/addEvent") {
        string name  = jsonGet(req.body, "name");
        string date  = jsonGet(req.body, "date");
        string venue = jsonGet(req.body, "venue");
        string p     = jsonGet(req.body, "price");
        string s     = jsonGet(req.body, "seats");
        int price = p.empty() ? 0 : stoi(p);
        int seats = s.empty() ? 0 : stoi(s);
        return textResp(bs.addEvent(name, date, venue, price, seats));
    }

    return httpResponse(404, "text/plain", "Not Found");
}


int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed\n"; return 1;
    }
#endif

    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == INVALID_SOCKET) { cerr << "socket() failed\n"; return 1; }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "bind() failed — port " << PORT << " already in use?\n";
        CLOSE_SOCKET(serverSock); return 1;
    }

    listen(serverSock, 10);

    cout << "\n";
    cout << "  Events  : " << EVENTS_FILE  << "\n";
    cout << "  Tickets : " << TICKETS_FILE << "\n";
    cout << "  Frontend: " << FRONTEND_DIR << "\n\n";

    {
        ifstream ef(EVENTS_FILE);
        ifstream ff(FRONTEND_DIR + "index.html");
        cout << (ef ? "  [OK] data/events.txt found\n"
                    : "  [ERR] data/events.txt NOT found — run from backend/!\n");
        cout << (ff ? "  [OK] frontend/index.html found\n"
                    : "  [ERR] frontend/index.html NOT found\n");
    }

    cout << "\n  Open: http://localhost:" << PORT << "\n\n";

#ifdef _WIN32
    system("start http://localhost:3000");
#endif

    cout << "  Waiting for requests...\n\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        SOCKET clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientLen);
        if (clientSock == INVALID_SOCKET) continue;

        
        string raw;
        char buf[4096];
        int received;
        while ((received = recv(clientSock, buf, sizeof(buf)-1, 0)) > 0) {
            buf[received] = '\0';
            raw += string(buf, received);

            size_t headerEnd = raw.find("\r\n\r\n");
            if (headerEnd == string::npos) continue;

            
            string headersLower = raw.substr(0, headerEnd);
            transform(headersLower.begin(), headersLower.end(), headersLower.begin(), ::tolower);
            size_t clPos = headersLower.find("content-length:");
            if (clPos == string::npos) break; // GET — no body

            size_t vs = clPos + 15;
            while (vs < headersLower.size() && headersLower[vs]==' ') ++vs;
            size_t ve = headersLower.find("\r\n", vs);
            int contentLength = 0;
            try { contentLength = stoi(headersLower.substr(vs, ve - vs)); } catch (...) { break; }

            int bodyReceived = (int)raw.size() - (int)(headerEnd + 4);
            if (bodyReceived >= contentLength) break;
        }

        if (!raw.empty()) {
            string response = handleRequest(raw);
            send(clientSock, response.c_str(), (int)response.size(), 0);
        }

        CLOSE_SOCKET(clientSock);
    }

    CLOSE_SOCKET(serverSock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}