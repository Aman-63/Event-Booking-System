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
#include <map>
#include <set>
#include <algorithm>
#include <cstring>

using namespace std;

const string EVENTS_FILE  = "../data/events.txt";
const string TICKETS_FILE = "../data/tickets.txt";
const string SEATMAP_FILE = "../data/seatmap.txt";
const string STATS_FILE   = "../data/stats.txt";
const string FRONTEND_DIR = "../frontend/";
const int    PORT         = 3000;

//  HELPERS
static bool fileExists(const string& path) {
    ifstream f(path);
    return f.good();
}

static bool fileEmpty(const string& path) {
    ifstream f(path);
    if (!f) return true;
    return f.peek() == ifstream::traits_type::eof();
}

//  DATA MODELS
class Event {
public:
    int id, seats, price;
    string name, date, venue;
    Event() : id(0), seats(0), price(0) {}
    Event(int i, string n, string d, string v, int p, int s)
        : id(i), name(n), date(d), venue(v), price(p), seats(s) {}
};

//  STATS  (persisted in stats.txt)
class Stats {
public:
    int    totalBookings = 0;
    int    totalSeats    = 0;
    long long totalRevenue  = 0;

    void load() {
        ifstream f(STATS_FILE);
        if (!f) return;
        string line;
        while (getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.find("bookings=") == 0) totalBookings = stoi(line.substr(9));
            else if (line.find("seats=")    == 0) totalSeats    = stoi(line.substr(6));
            else if (line.find("revenue=")  == 0) totalRevenue  = stoll(line.substr(8));
        }
    }

    void save() {
        ofstream f(STATS_FILE);
        f << "bookings=" << totalBookings << "\n"
          << "seats="    << totalSeats    << "\n"
          << "revenue="  << totalRevenue  << "\n";
    }

    void reset() {
        totalBookings = 0;
        totalSeats    = 0;
        totalRevenue  = 0;
        save();
    }

    string toJSON() {
        ostringstream out;
        out << "{"
            << "\"bookings\":" << totalBookings << ","
            << "\"seats\":"    << totalSeats    << ","
            << "\"revenue\":"  << totalRevenue
            << "}";
        return out.str();
    }
};

//  SEAT MAP  
class SeatMap {
    map<int, vector<string>> bookedSeats; 

public:
    void load() {
        bookedSeats.clear();
        ifstream f(SEATMAP_FILE);
        if (!f) return;
        string line;
        while (getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            size_t sep = line.find('|');
            if (sep == string::npos) continue;
            try {
                int eid = stoi(line.substr(0, sep));
                string seats = line.substr(sep + 1);
                vector<string> sv;
                istringstream ss(seats);
                string s;
                while (getline(ss, s, ',')) {
                    if (!s.empty()) sv.push_back(s);
                }
                bookedSeats[eid] = sv;
            } catch (...) {}
        }
    }

    void save() {
        ofstream f(SEATMAP_FILE);
        for (auto& kv : bookedSeats) {
            f << kv.first << "|";
            for (size_t i = 0; i < kv.second.size(); ++i) {
                if (i) f << ",";
                f << kv.second[i];
            }
            f << "\n";
        }
    }

    void reset() {
        bookedSeats.clear();
        save();
    }

    // Add booked seats for an event
    void addSeats(int eventId, const vector<string>& seats) {
        auto& v = bookedSeats[eventId];
        for (auto& s : seats) v.push_back(s);
        save();
    }

    // Remove seats when cancelled (removes first N matching seats)
    void removeSeats(int eventId, int count) {
        auto it = bookedSeats.find(eventId);
        if (it == bookedSeats.end()) return;
        auto& v = it->second;
        int toRemove = min(count, (int)v.size());
        v.erase(v.begin(), v.begin() + toRemove);
        if (v.empty()) bookedSeats.erase(it);
        save();
    }

    // Get booked seats for an event as JSON array of strings
    string getJSON(int eventId) {
        auto it = bookedSeats.find(eventId);
        if (it == bookedSeats.end()) return "[]";
        ostringstream out;
        out << "[";
        auto& v = it->second;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) out << ",";
            out << "\"" << v[i] << "\"";
        }
        out << "]";
        return out.str();
    }
};

//  BOOKING SYSTEM
class BookingSystem {
    vector<Event> events;
    Stats    stats;
    SeatMap  seatMap;

    void loadEvents() {
        events.clear();
        ifstream f(EVENTS_FILE);
        if (!f) {
            cerr << "  [ERROR] Cannot open: " << EVENTS_FILE << "\n"
                 << "  Run server.exe from inside backend/!\n";
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

    // If tickets.txt is missing or empty, reset seatmap and stats too
    void syncWithTickets() {
        if (!fileExists(TICKETS_FILE) || fileEmpty(TICKETS_FILE)) {
            cout << "  [SYNC] tickets.txt is empty/missing — resetting seatmap and stats\n";
            seatMap.reset();
            stats.reset();
        }
    }

public:
    BookingSystem() {
        stats.load();
        seatMap.load();
        syncWithTickets();
    }

    // GET /events
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

    // GET /stats 
    string getStatsJSON() {
        syncWithTickets();
        return stats.toJSON();
    }

    // GET /seatmap?eventId=N 
    string getSeatMapJSON(int eventId) {
        seatMap.load();
        return seatMap.getJSON(eventId);
    }

    // POST /book
    // seatLabels: comma-separated string like "A1,A2,B3" or empty for concert zones
    string book(int eventId, const string& user, int seats, const string& seatLabels) {
        cout << "  [BOOK] eventId=" << eventId << " user=" << user
             << " seats=" << seats << " labels=" << seatLabels << "\n";
        syncWithTickets();
        loadEvents();
        for (auto& e : events) {
            if (e.id == eventId) {
                cout << "  [BOOK] Found '" << e.name << "' available=" << e.seats << "\n";
                if (e.seats >= seats) {
                    e.seats -= seats;
                    saveEvents();

                    // Save to tickets.txt
                    ofstream f(TICKETS_FILE, ios::app);
                    if (!f) return "FILE_ERROR";
                    f << user << " " << eventId << " " << seats << "\n";

                    // Save seat labels to seatmap
                    if (!seatLabels.empty()) {
                        vector<string> sv;
                        istringstream ss(seatLabels);
                        string s;
                        while (getline(ss, s, ','))
                            if (!s.empty()) sv.push_back(s);
                        seatMap.load();
                        seatMap.addSeats(eventId, sv);
                    }

                    // Update stats
                    stats.totalBookings++;
                    stats.totalSeats   += seats;
                    stats.totalRevenue += (long long)seats * e.price;
                    stats.save();

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

    // POST /cancel 
    string cancel(const string& user, int eventId, int seats) {
        syncWithTickets();
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

        // Remove seats from seatmap
        seatMap.load();
        seatMap.removeSeats(eventId, seats);

        // Update stats
        loadEvents(); // reload to get price
        int price = 0;
        for (auto& e : events) if (e.id == eventId) price = e.price;
        stats.totalSeats    -= seats;
        stats.totalRevenue  -= (long long)seats * price;
        if (stats.totalSeats   < 0) stats.totalSeats   = 0;
        if (stats.totalRevenue < 0) stats.totalRevenue = 0;
        stats.save();

        cout << "  [CANCELLED] " << user << " x" << seats << "\n";
        return "CANCELLED";
    }

    // GET /history
    string getHistoryJSON() {
        syncWithTickets();
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
                << "\"user\":\""      << u      << "\","
                << "\"eventId\":"     << eid    << ","
                << "\"eventName\":\"" << evName << "\","
                << "\"seats\":"       << s
                << "}";
        }
        out << "]";
        return out.str();
    }

    // POST /addEvent 
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

//  HTTP HELPERS
string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == string::npos) ? "" : s.substr(a, b - a + 1);
}

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

// Parse query string param: /seatmap?eventId=3 → "3"
string queryParam(const string& path, const string& key) {
    size_t q = path.find('?');
    if (q == string::npos) return "";
    string qs = path.substr(q + 1);
    string needle = key + "=";
    size_t pos = qs.find(needle);
    if (pos == string::npos) return "";
    pos += needle.size();
    size_t end = qs.find('&', pos);
    return qs.substr(pos, end == string::npos ? string::npos : end - pos);
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

string jsonResp(const string& b){ return httpResponse(200,"application/json",b); }
string textResp(const string& b){ return httpResponse(200,"text/plain",b); }
string htmlResp(const string& b){ return httpResponse(200,"text/html",b); }
string cssResp (const string& b){ return httpResponse(200,"text/css",b); }
string jsResp  (const string& b){ return httpResponse(200,"application/javascript",b); }

//  REQUEST PARSER

struct HttpRequest {
    string method, path, body;
    int contentLength = 0;
};

HttpRequest parseRequest(const string& raw) {
    HttpRequest req;
    size_t lineEnd = raw.find("\r\n");
    if (lineEnd == string::npos) lineEnd = raw.find("\n");
    if (lineEnd == string::npos) return req;

    istringstream fl(raw.substr(0, lineEnd));
    string proto;
    fl >> req.method >> req.path >> proto;

    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == string::npos) {
        headerEnd = raw.find("\n\n");
        if (headerEnd == string::npos) return req;
        req.body = raw.substr(headerEnd + 2);
    } else {
        req.body = raw.substr(headerEnd + 4);
    }

    string headers = raw.substr(0, headerEnd);
    string hl = headers;
    transform(hl.begin(), hl.end(), hl.begin(), ::tolower);
    size_t clPos = hl.find("content-length:");
    if (clPos != string::npos) {
        size_t vs = clPos + 15;
        while (vs < headers.size() && headers[vs]==' ') ++vs;
        size_t ve = headers.find("\r\n", vs);
        try { req.contentLength = stoi(headers.substr(vs, ve-vs)); } catch(...) {}
    }
    if (req.contentLength > 0 && (int)req.body.size() > req.contentLength)
        req.body = req.body.substr(0, req.contentLength);

    return req;
}

//  REQUEST HANDLER

BookingSystem bs;

string handleRequest(const string& raw) {
    HttpRequest req = parseRequest(raw);

    // Path without query string for routing
    string routePath = req.path;
    size_t q = routePath.find('?');
    if (q != string::npos) routePath = routePath.substr(0, q);

    cout << "  --> " << req.method << " " << req.path << "\n";
    if (!req.body.empty()) cout << "  --> body: " << req.body << "\n";

    if (req.method == "OPTIONS") return httpResponse(200,"text/plain","");

    //  Static files 
    if (req.method=="GET" && (routePath=="/" || routePath=="/index.html")) {
        string c = readFile(FRONTEND_DIR+"index.html");
        return c.empty() ? httpResponse(404,"text/plain","index.html not found") : htmlResp(c);
    }
    if (req.method=="GET" && routePath=="/style.css") {
        string c = readFile(FRONTEND_DIR+"style.css");
        return c.empty() ? httpResponse(404,"text/plain","style.css not found") : cssResp(c);
    }
    if (req.method=="GET" && routePath=="/script.js") {
        string c = readFile(FRONTEND_DIR+"script.js");
        return c.empty() ? httpResponse(404,"text/plain","script.js not found") : jsResp(c);
    }

    // API
    if (req.method=="GET" && routePath=="/events")  return jsonResp(bs.getEventsJSON());
    if (req.method=="GET" && routePath=="/history") return jsonResp(bs.getHistoryJSON());
    if (req.method=="GET" && routePath=="/stats")   return jsonResp(bs.getStatsJSON());

    if (req.method=="GET" && routePath=="/seatmap") {
        string eid = queryParam(req.path, "eventId");
        int eventId = eid.empty() ? 0 : stoi(eid);
        return jsonResp(bs.getSeatMapJSON(eventId));
    }

    if (req.method=="POST" && routePath=="/book") {
        string user       = jsonGet(req.body, "user");
        string eid        = jsonGet(req.body, "eventId");
        string s          = jsonGet(req.body, "seats");
        string seatLabels = jsonGet(req.body, "seatLabels");
        cout << "  --> user='" << user << "' eventId='" << eid
             << "' seats='" << s << "' labels='" << seatLabels << "'\n";
        int eventId = eid.empty() ? 0 : stoi(eid);
        int seats   = s.empty()   ? 0 : stoi(s);
        return textResp(bs.book(eventId, user, seats, seatLabels));
    }

    if (req.method=="POST" && routePath=="/cancel") {
        string user = jsonGet(req.body, "user");
        string eid  = jsonGet(req.body, "eventId");
        string s    = jsonGet(req.body, "seats");
        int eventId = eid.empty() ? 0 : stoi(eid);
        int seats   = s.empty()   ? 0 : stoi(s);
        return textResp(bs.cancel(user, eventId, seats));
    }

    if (req.method=="POST" && routePath=="/addEvent") {
        string name  = jsonGet(req.body, "name");
        string date  = jsonGet(req.body, "date");
        string venue = jsonGet(req.body, "venue");
        string p     = jsonGet(req.body, "price");
        string s     = jsonGet(req.body, "seats");
        int price = p.empty() ? 0 : stoi(p);
        int seats = s.empty() ? 0 : stoi(s);
        return textResp(bs.addEvent(name, date, venue, price, seats));
    }

    return httpResponse(404,"text/plain","Not Found");
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) { cerr << "WSAStartup failed\n"; return 1; }
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
        cerr << "bind() failed — port " << PORT << " in use?\n";
        CLOSE_SOCKET(serverSock); return 1;
    }
    listen(serverSock, 10);

    cout << "  Events  : " << EVENTS_FILE  << "\n"
         << "  Tickets : " << TICKETS_FILE << "\n"
         << "  SeatMap : " << SEATMAP_FILE << "\n"
         << "  Stats   : " << STATS_FILE   << "\n\n";

    { ifstream ef(EVENTS_FILE); ifstream ff(FRONTEND_DIR+"index.html");
      cout << (ef ? "  [OK] events.txt found\n"      : "  [ERR] events.txt NOT found — run from backend/!\n");
      cout << (ff ? "  [OK] index.html found\n\n"    : "  [ERR] index.html NOT found\n\n"); }

    cout << "  Open: http://localhost:" << PORT << "\n\n";

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
            string hl = raw.substr(0, headerEnd);
            transform(hl.begin(), hl.end(), hl.begin(), ::tolower);
            size_t clPos = hl.find("content-length:");
            if (clPos == string::npos) break;
            size_t vs = clPos + 15;
            while (vs < hl.size() && hl[vs]==' ') ++vs;
            size_t ve = hl.find("\r\n", vs);
            int cl = 0;
            try { cl = stoi(hl.substr(vs, ve-vs)); } catch(...) { break; }
            if ((int)raw.size() - (int)(headerEnd+4) >= cl) break;
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