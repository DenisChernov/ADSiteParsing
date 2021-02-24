#ifdef WIN32
    #define CURL_STATICLIB // используется статическая сборка библиотеки
#endif

#include <cstdio>
#include <curl/curl.h>

#ifdef WIN32
    #include <windows.h>
#endif

#include <iostream>
#include <cstring>
#include <regex>
#include <cstdio>
#include <iomanip>
#include <fstream>
#include <vector>

#ifdef WIN32
#ifdef _DEBUG
#pragma comment(lib,"libcurld.lib")
#else
#pragma comment(lib,"libcurl.lib")
#endif

#pragma comment(lib, "crypt32")
#pragma comment(lib,"ws2_32.lib")  // Зависимость от WinSocks2
#pragma comment(lib,"wldap32.lib")
#endif

using namespace std;

enum WORK_TYPES {
    TEST,
    PARSE_URL,
    PARSE_LOCAL
};

struct itemAddress {
    string address;
    string georef;
};

struct itemPrice {
    string price;
    string currency;
};

struct offer {
    string id;
    string href;
    string pageTitle;
    vector<string>previewThumbImages;
    string offerTitle;
    itemPrice price;
    itemAddress address;
    string shortDescription;
    string lastUpdate;
};

struct page {
    string href;
    int pageNum{};
};

size_t callbackWriteBufer(char *ptr, size_t size, size_t nmemb, string *data) {
    if (data) {
        data->append(ptr, size * nmemb);
        return size * nmemb;
    }
    return 0;
}

page findPageCount(const string& buffer) {
    page p;
    size_t posStart = buffer.rfind("a class=\"pagination-page\"");
    size_t posEnd = buffer.find("\">", posStart);
    string str = buffer.substr(posStart,  posEnd - posStart);

    cout << "source: " << str << endl;
    const regex pagination(R"(([\w\-\/]*)\?p=(\d{1,3}))");
    smatch match;
    if (regex_search(str, match, pagination)) {
        cout << match[1] << "  " << match[2] << endl;
        p.href = match[1];
        p.pageNum = stoi(match[2]);
    } else {
        cout << "not found anything" << endl;
    }

    return p;
}

void getOffersOnPage(const string& buffer, vector<offer>* offers) {

    smatch item_match;
    smatch match;
    const regex offerItem("(data-marker=\"item\")");
    const regex itemID("data-item-id=\"(\\d*)\"");
    const regex itemDetailHref("itemProp=\"url\" href=\"([\\w\\-\\/\\.]*)\"");
    const regex itemDetailPageTitle("title=\"(.*)\"\\srel=\"noopener\">");
    const regex itemPreviewThumb("data-marker=\"slider-image\\/image-([\\w\\-\\/\\.\\:]*)\">");
    const regex itemTitle(R"lit(title="([^"]*)"\sitemprop="url")lit", std::regex_constants::icase);
    const regex itemPrice(R"lit(itemProp="priceCurrency"\scontent="([^"]*)".*itemProp="price"\scontent="([^"]*)")lit");
    const regex itemAddress("geo-address[^>]*><span>([^<\\/span]*).*geo-georeferences[^>]*><span>([^<]*)<\\/span>");
    const regex itemShortDescription("iva-item-description[^>]*>([^<]*)");
    const regex itemLastUpdate("data-marker=\"item-date\"[^>]*>([^<]*)<");

    string::const_iterator searchStart = buffer.cbegin();
    string::const_iterator offerEnd;
    offer item;

    while (regex_search(searchStart, buffer.cend(), match, offerItem)) {
        searchStart = match.suffix().first;

        regex_search(searchStart, buffer.cend(), match, offerItem);
        offerEnd = match.suffix().first;

        if (regex_search(searchStart, offerEnd, match, itemID)) {
            searchStart = match.suffix().first;
            item.id = match[1];
        } else {
            item.id.clear();
        }

        if (regex_search(searchStart, offerEnd, match, itemDetailHref)) {
            searchStart = match.suffix().first;
            item.href = match[1];
        } else {
            item.href.clear();
        }

        if (regex_search(searchStart, offerEnd, match, itemDetailPageTitle)) {
            searchStart = match.suffix().first;
            item.pageTitle = match[1];
        } else {
            item.pageTitle.clear();
        }

        /** TODO:
         *    Сбор всех превью.
         *
        */

        if (regex_search(searchStart, offerEnd, match, itemPreviewThumb)) {
            searchStart = match.suffix().first;
            item.previewThumbImages.push_back(match[1]);
        } else {
            item.previewThumbImages.clear();
        }

        if (regex_search(searchStart, offerEnd, match, itemTitle)) {
            searchStart = match.suffix().first;
            item.offerTitle = match[1];
        } else {
            item.offerTitle.clear();
        }

        if (regex_search(searchStart, offerEnd, match, itemPrice)) {
            searchStart = match.suffix().first;
            item.price.price = match[2];
            item.price.currency = match[1];
        } else {
            item.price.price.clear();
            item.price.currency.clear();
        }

        if (regex_search(searchStart, offerEnd, match, itemAddress)) {
            searchStart = match.suffix().first;
            item.address.address = match[1];
            item.address.georef = match[2];
        } else {
            item.address.address.clear();
            item.address.georef.clear();
        }

        if (regex_search(searchStart, offerEnd, match, itemShortDescription)) {
            searchStart = match.suffix().first;
            item.shortDescription = match[1];
        } else {
            item.shortDescription.clear();
        }

        if (regex_search(searchStart, offerEnd, match, itemLastUpdate)) {
            searchStart = match.suffix().first;
            item.lastUpdate = match[1];
        } else {
            item.lastUpdate.clear();
        }

        offers->push_back(item);
    }
}

int testParsing() {
    vector<page> pages;
    ifstream in("nedvizhimost.log", ios::binary);
    if (!in.is_open()) {

        CURL *curl;
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
        if (!curl) {
            perror("curl init error");
            curl_global_cleanup();

            return -1;
        }
        const string url = "https://www.avito.ru/murmansk/kvartiry/prodam-ASgBAgICAUSSA8YQ";
        string buffer;

        curl_slist *headersList = nullptr;
        headersList = curl_slist_append(headersList, "Accept: */*");
        headersList = curl_slist_append(headersList, "Upgrade-Insecure-Requests: 1");
        curl_easy_setopt(curl, CURLOPT_USERAGENT,
                         "Mozilla/5.0 (Macintosh; Intel Mac OS X 11_1_0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4324.96 Safari/537.36");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callbackWriteBufer);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookie.txt");
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);

        CURLcode result = curl_easy_perform(curl);
        if (result != CURLE_OK) {
            curl_slist_free_all(headersList);
            curl_easy_cleanup(curl);
            curl_global_cleanup();

            return -1;
        }

        size_t posStart = buffer.find("data-marker=\"item\"");
        size_t posEnd = buffer.find("Рекомендованные объявления");
        buffer = buffer.substr(posStart, buffer.length() - (posStart + posEnd));

        FILE *page;
#ifdef WIN32
        fopen_s(&page, "nedvizhimost.log", "wb");
#else
        page = fopen64("nedvizhimost.log", "wb");
#endif
        fwrite(buffer.c_str(), 1, buffer.length(), page);
        fclose(page);

        curl_slist_free_all(headersList);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
    } else {
        int size = in.seekg(0, ios::end).tellg();
        in.seekg(0);
        char *buf = new char[size + 1];
        in.read(buf, size);
        buf[size] = 0;
        string buffer = buf;

        const page maxPage = findPageCount(buffer);
        vector<offer> offers;
        getOffersOnPage(buffer, &offers);

        cout << "items: " << offers.size() << endl;
    }
    if (in.is_open()) {
        in.close();
    }

    return 0;
}

int parseUrl(const string& startUrl) {
    CURL *curl;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl) {
        perror("curl init error");
        curl_global_cleanup();

        return -1;
    }

    string buffer;
    curl_slist *headersList = nullptr;
    headersList = curl_slist_append(headersList, "Accept: */*");
    headersList = curl_slist_append(headersList, "Upgrade-Insecure-Requests: 1");
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
                     "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_5_0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4324.96 Safari/535.36");
    curl_easy_setopt(curl, CURLOPT_URL, startUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callbackWriteBufer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookie.txt");
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
    curl_easy_setopt(curl, CURLOPT_PROXY, "http://154.16.202.22:8080/");

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        curl_slist_free_all(headersList);
        curl_easy_cleanup(curl);
        curl_global_cleanup();

        return -1;
    }

    size_t posStart = buffer.find("data-marker=\"item\"");
    size_t posEnd = buffer.find("Рекомендованные объявления");
    buffer = buffer.substr(posStart, buffer.length() - (posStart + posEnd));

    const page maxPage = findPageCount(buffer);
    vector<offer> offers;
    getOffersOnPage(buffer, &offers);
    string outFilename;

    FILE *page;
    outFilename = "page_1.log";
#ifdef WIN32
    fopen_s(&page, outFilename.c_str(), "wb");
#else
    page = fopen64(outFilename.c_str(), "wb");
#endif
    fwrite(buffer.c_str(), 1, buffer.length(), page);
    fclose(page);

    if (maxPage.pageNum > 1) {
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "cookie.txt");
        string currentUrl;
        for (int pageNum = 2; pageNum <= maxPage.pageNum; ++pageNum) {
            buffer.clear();
            currentUrl = startUrl + "?p=" + to_string(pageNum);
            curl_easy_setopt(curl, CURLOPT_URL, currentUrl.c_str());

            result = curl_easy_perform(curl);

            if (result != CURLE_OK) {
                cout << "error curl" << endl;
                return -1;
            } else {
                posStart = buffer.find("data-marker=\"item\"");
                posEnd = buffer.find("pagination-button/prev");
                buffer = buffer.substr(posStart, buffer.length() - (posStart + posEnd));

                outFilename = "page_" + to_string(pageNum) + ".log";
#ifdef WIN32
                fopen_s(&page, outFilename.c_str(), "wb");
#else
                page = fopen64(outFilename.c_str(), "wb");
#endif
                fwrite(buffer.c_str(), 1, buffer.length(), page);
                fclose(page);

                getOffersOnPage(buffer, &offers);

            }
        }
    }

    cout << "Объявлений получено: " << offers.size() << endl;

    curl_slist_free_all(headersList);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
}

int parseLocalFiles() {
    vector<offer> offers;

    for (int pageNum = 1; ; ++pageNum) {
        ifstream in("page_" + to_string(pageNum), ios::binary);
        if (in.is_open()) {
            int size = in.seekg(0, ios::end).tellg();
            in.seekg(0);
            char *buf = new char[size + 1];
            in.read(buf, size);
            buf[size] = 0;
            string buffer = buf;

            getOffersOnPage(buffer, &offers);
            in.close();
        } else {
            break;
        }
    }

    cout << "total: " << offers.size() << endl;

    return 0;
}

int main(int argc, char* argv[]) {
#ifdef WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    WORK_TYPES work = TEST;

    if (argc > 1) {
        if (strncmp(argv[1], "--getAll", 8) == 0) {
            work = PARSE_URL;
        } else if (strncmp(argv[1], "--parseLocal", 12) == 0) {
            work = PARSE_LOCAL;
        }
    }

    switch (work) {
        case TEST: {
            cout << "Тест" << endl;
            testParsing();
            break;
        }
        case PARSE_URL: {
            cout << "Получаю все объявления" << endl;
            parseUrl(argv[2]);
            break;
        }
        case PARSE_LOCAL: {
            cout << "Тест локальных объявлений" << endl;
            parseLocalFiles();
            break;
        }
    }

    return 0;
}
