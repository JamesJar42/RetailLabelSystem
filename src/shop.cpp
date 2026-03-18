#include "../include/shop.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <QDebug>
#include <fstream>
#include <sstream>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>

std::string shop::getLastOAuthError() const
{
    return lastOAuthError;
}

bool shop::validateCloverConnection(const std::string &merchantId, const std::string &apiToken, bool isSandbox)
{
    lastOAuthError.clear();
    if (apiToken.empty()) {
        lastOAuthError = "Missing Clover API token.";
        return false;
    }

    QNetworkAccessManager manager;
    const QString token = QString::fromStdString(apiToken);
    const QString expectedMerchant = QString::fromStdString(merchantId);
    const QStringList hosts = isSandbox
        ? QStringList{ "https://apisandbox.dev.clover.com", "https://sandbox.dev.clover.com" }
        : QStringList{ "https://api.clover.com", "https://www.clover.com" };

    for (const QString &host : hosts) {
        QUrl url(host + "/v3/merchants/current");
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
        request.setRawHeader("Accept", "application/json");

        QNetworkReply *reply = manager.get(request);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            lastOAuthError = QString("Validation failed host=%1 status=%2 error=%3 body=%4")
                .arg(host)
                .arg(statusCode)
                .arg(reply->errorString())
                .arg(QString::fromUtf8(body))
                .toStdString();
            reply->deleteLater();
            continue;
        }
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(body).object();
        const QString currentMerchant = root.value("id").toString();
        if (!expectedMerchant.isEmpty() && !currentMerchant.isEmpty() && expectedMerchant != currentMerchant) {
            lastOAuthError = QString("Token valid but merchant mismatch. expected=%1 actual=%2")
                .arg(expectedMerchant, currentMerchant)
                .toStdString();
            return false;
        }

        if (currentMerchant.isEmpty()) {
            lastOAuthError = QString("Validation response missing merchant id. host=%1 body=%2")
                .arg(host)
                .arg(QString::fromUtf8(body))
                .toStdString();
            return false;
        }

        return true;
    }

    if (lastOAuthError.empty()) {
        lastOAuthError = "Clover connection validation failed with unknown error.";
    }
    return false;
}

void shop::add(product pd)
{
    if (barcodeIndex.find(pd.getBarcode()) == barcodeIndex.end()) {
        barcodeIndex[pd.getBarcode()] = database.size();
    }
    database.push_back(pd);
}

std::vector<product>& shop::listProduct()
{
    // qDebug() << "listProduct called. Database size:" << database.size(); 
    // Commented out debug to reduce console spam on large lists
    return database;
}

product& shop::search(int choice)
{
    return  database.at(choice-1);;
}

void shop::removeByBarcode(const std::string &barcode)
{
    auto it = std::remove_if(database.begin(), database.end(), [&](const product &p) {
        return p.getBarcode() == barcode;
    });
    
    bool changed = (it != database.end());
    database.erase(it, database.end());

    if (changed) {
        // Rebuild index
        barcodeIndex.clear();
        for (size_t i = 0; i < database.size(); ++i) {
            const std::string& bc = database[i].getBarcode();
            if (barcodeIndex.find(bc) == barcodeIndex.end()) {
                barcodeIndex[bc] = i;
            }
        }
    }
}

product& shop::searchByBarcode(const std::string &barcode)
{
    auto it = barcodeIndex.find(barcode);
    if (it != barcodeIndex.end()) {
        return database[it->second];
    }
    
    // Fallback or throw
    throw std::runtime_error("Product with the given barcode not found.");
}

void shop::clear()
{
    database.clear();
    barcodeIndex.clear();
}

bool shop::loadFromCSV(const std::string &path, const CSVMapping &mapping)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;

    // Estimate lines
    file.seekg(0, std::ios::end);
    std::streampos length = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (length > 0) {
        size_t estLines = static_cast<size_t>(length) / 50; 
        database.reserve(estLines);
        barcodeIndex.reserve(estLines);
    }

    clear();
    std::string line;
    bool firstLine = true;
    
    // Buffer optimization if possible, but basic implementation first
    int maxIdx = std::max({mapping.barcodeCol, mapping.nameCol, mapping.priceCol, mapping.originalPriceCol, mapping.labelQuantityCol});
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (firstLine && mapping.hasHeader) {
            firstLine = false;
            continue;
        }
        firstLine = false; 
        
        // Manual CSV parsing
        std::vector<std::string> parts;
        parts.reserve(maxIdx + 1);
        
        size_t start = 0;
        size_t end = line.find(',');
        
        while (end != std::string::npos) {
            parts.emplace_back(line, start, end - start);
            start = end + 1;
            end = line.find(',', start);
        }
        parts.emplace_back(line, start); // Last part

        auto getPart = [&](int idx) -> std::string {
            if (idx >= 0 && idx < parts.size()) return parts[idx];
            return "";
        };

        try {
            std::string barcode = getPart(mapping.barcodeCol);
            if (barcode.empty()) continue;

            // Heuristic: If the barcode is literally "Barcode", skip this line (it's a header)
            // regardless of the mapping.hasHeader setting.
            if (barcode == "Barcode" || barcode == "barcode") continue;

            std::string name = getPart(mapping.nameCol);
            
            float price = 0.0f;
            const std::string& pStr = getPart(mapping.priceCol);
            if (!pStr.empty()) {
                try { price = std::stof(pStr); } catch(...) {}
            }

            // Size logic removed
            
            float originalPrice = 0.0f;
            const std::string& opStr = getPart(mapping.originalPriceCol);
            if (!opStr.empty()) {
                try { originalPrice = std::stof(opStr); } catch(...) {}
            }

            int labelQuantity = 0;
            const std::string& lqStr = getPart(mapping.labelQuantityCol);
            if (!lqStr.empty()) {
                try {
                    labelQuantity = std::stoi(lqStr);
                } catch (...) {
                    if(lqStr == "true" || lqStr == "True") labelQuantity = 1;
                }
            }

            add(product(name, price, barcode, labelQuantity, originalPrice));
        } catch (...) {
            continue;
        }
    }
    file.close();
    return true;
}

bool shop::saveToCSV(const std::string &path)
{
    std::ofstream file(path);
    if (!file.is_open()) return false;

    // Header
    file << "Barcode,Name,Price,OriginalPrice,LabelFlag\n";

    for (const auto &p : database) {
        file << p.getBarcode() << ","
             << p.getName() << ","
             << p.getPrice() << ","
             << p.getOriginalPrice() << ","
             << p.getLabelQuantity() << "\n";
    }
    file.close();
    return true;
}


bool shop::loadFromClover(const std::string &merchantId, const std::string &apiToken, bool isSandbox)
{
    // Use QEventLoop to make the async network request synchronous for this function
    QNetworkAccessManager manager;
    QString mId = QString::fromStdString(merchantId);
    QString token = QString::fromStdString(apiToken);
    
    // Choose base URL based on environment
    QString baseUrl = isSandbox ? "https://sandbox.dev.clover.com" : "https://api.clover.com";

    // Track existing barcodes to identify new items
    std::unordered_set<std::string> existingBarcodes;
    for (const auto &p : database) {
        existingBarcodes.insert(p.getBarcode());
    }

    std::vector<product> newDatabase;
    int offset = 0;
    const int limit = 100;
    bool moreData = true;

    while (moreData) {
        QUrl url(QString("%1/v3/merchants/%2/items?limit=%3&offset=%4").arg(baseUrl).arg(mId).arg(limit).arg(offset));
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
        request.setRawHeader("Accept", "application/json");

        QNetworkReply *reply = manager.get(request);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Clover API Error:" << reply->errorString();
            reply->deleteLater();
            return false;
        }

        QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root = doc.object();
        
        if (!root.contains("elements")) {
             // Maybe end of list or error?
             break;
        }

        QJsonArray elements = root["elements"].toArray();
        if (elements.isEmpty()) {
            moreData = false;
            break;
        }

        for (const auto &val : elements) {
            QJsonObject item = val.toObject();
            
            // Skip deleted or hidden items
            if (item.value("deleted").toBool(false)) continue;
            // hidden items might still need labels? Let's include them for now or make it optional. 
            // Usually hidden means not on register, but might be in backroom needing label.

            // Extract fields
            QString name = item["name"].toString();
            
            // Barcode resolution: SKU -> Code -> ID
            QString barcode = item["sku"].toString();
            if (barcode.isEmpty()) barcode = item["code"].toString();
            if (barcode.isEmpty()) barcode = item["id"].toString(); // Fallback to Clover ID
            
            // Clover price is in cents
            double price = 0.0;
            if (item.contains("price")) {
                price = item["price"].toDouble() / 100.0;
            }

            
            // Size logic removed
            
            int qty = 0; 
            if (item.contains("stockCount")) qty = item["stockCount"].toInt(); 
            
            // If item is new, automatically flag for labelling (set qty to at least 1)
            if (existingBarcodes.find(barcode.toStdString()) == existingBarcodes.end()) {
                if (qty <= 0) qty = 1;
            }

            if (!barcode.isEmpty()) {
                product p(name.toStdString(), (float)price, barcode.toStdString(), qty, 0.0f);
                newDatabase.push_back(p);
            }
        }
        
        offset += limit;
        if(offset > 10000) moreData = false; 
    }

    if (!newDatabase.empty()) {
        clear();
        database = std::move(newDatabase);
        
        // Rebuild index for the new database
        barcodeIndex.reserve(database.size());
        for (size_t i = 0; i < database.size(); ++i) {
             const std::string& bc = database[i].getBarcode();
             if (barcodeIndex.find(bc) == barcodeIndex.end()) {
                 barcodeIndex[bc] = i;
             }
        }
        return true;
    }
    
    return false;
}




std::pair<std::string, std::string> shop::exchangeCloverAuthCode(
    const std::string &clientId,
    const std::string &clientSecret,
    const std::string &code,
    bool isSandbox,
    const std::string &codeVerifier) {
    QNetworkAccessManager manager;
    lastOAuthError.clear();

    const QStringList tokenHosts = isSandbox
        ? QStringList{ "https://apisandbox.dev.clover.com", "https://sandbox.dev.clover.com" }
        : QStringList{ "https://api.clover.com", "https://www.clover.com" };

    auto attemptTokenExchange = [&](const QString &host,
                                    const QByteArray &body,
                                    const QByteArray &contentType,
                                    QString &tokenOut,
                                    QString &merchantOut,
                                    QByteArray &rawOut,
                                    int &statusOut,
                                    QString &errOut) -> bool {
        QUrl url(host + "/oauth/v2/token");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);

        QNetworkReply *reply = manager.post(request, body);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        statusOut = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        rawOut = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            errOut = reply->errorString();
            reply->deleteLater();
            return false;
        }
        reply->deleteLater();

        const QJsonObject root = QJsonDocument::fromJson(rawOut).object();
        tokenOut = root.value("access_token").toString();
        merchantOut = root.value("merchant_id").toString();
        if (merchantOut.isEmpty()) merchantOut = root.value("merchantId").toString();
        return !tokenOut.isEmpty();
    };

    const QString qClientId = QString::fromStdString(clientId);
    const QString qClientSecret = QString::fromStdString(clientSecret);
    const QString qCode = QString::fromStdString(code);
    const QString qVerifier = QString::fromStdString(codeVerifier);

    // PKCE-first payloads (low-trust flow)
    QJsonObject pkceJson;
    pkceJson.insert("client_id", qClientId);
    pkceJson.insert("code", qCode);
    if (!qVerifier.isEmpty()) pkceJson.insert("code_verifier", qVerifier);
    const QByteArray pkceJsonBody = QJsonDocument(pkceJson).toJson(QJsonDocument::Compact);

    QUrlQuery pkceForm;
    pkceForm.addQueryItem("client_id", qClientId);
    pkceForm.addQueryItem("code", qCode);
    if (!qVerifier.isEmpty()) pkceForm.addQueryItem("code_verifier", qVerifier);
    const QByteArray pkceFormBody = pkceForm.toString(QUrl::FullyEncoded).toUtf8();

    // High-trust payloads (fallback)
    QJsonObject trustJson;
    trustJson.insert("client_id", qClientId);
    trustJson.insert("code", qCode);
    if (!qClientSecret.isEmpty()) trustJson.insert("client_secret", qClientSecret);
    const QByteArray trustJsonBody = QJsonDocument(trustJson).toJson(QJsonDocument::Compact);

    QUrlQuery trustForm;
    trustForm.addQueryItem("client_id", qClientId);
    trustForm.addQueryItem("code", qCode);
    if (!qClientSecret.isEmpty()) trustForm.addQueryItem("client_secret", qClientSecret);
    const QByteArray trustFormBody = trustForm.toString(QUrl::FullyEncoded).toUtf8();

    struct Variant {
        QString name;
        QByteArray body;
        QByteArray contentType;
    };
    QList<Variant> variants;
    variants.push_back({"pkce-json", pkceJsonBody, "application/json"});
    variants.push_back({"pkce-form", pkceFormBody, "application/x-www-form-urlencoded"});
    if (!qClientSecret.isEmpty()) {
        variants.push_back({"hightrust-json", trustJsonBody, "application/json"});
        variants.push_back({"hightrust-form", trustFormBody, "application/x-www-form-urlencoded"});
    }

    QString token;
    QString mId;
    QByteArray data;
    int statusCode = 0;
    QString err;
    bool success = false;

    for (const QString &host : tokenHosts) {
        for (const Variant &v : variants) {
            const bool ok = attemptTokenExchange(host, v.body, v.contentType, token, mId, data, statusCode, err);
            if (ok) {
                qDebug() << "OAuth Token Exchange Success with" << host << v.name << "status:" << statusCode;
                success = true;
                break;
            }

            const QString detail = QString("OAuth token attempt failed host=%1 variant=%2 status=%3 error=%4 body=%5")
                .arg(host, v.name)
                .arg(statusCode)
                .arg(err)
                .arg(QString::fromUtf8(data));
            qDebug() << detail;
            lastOAuthError = detail.toStdString();
        }
        if (success) break;
    }

    if (!success || token.isEmpty()) {
        if (lastOAuthError.empty()) {
            lastOAuthError = "OAuth token exchange failed with no detailed error body.";
        }
        return {};
    }
    
    // If merchant ID is missing, try fetch it with the new token
    if (mId.isEmpty()) {
        // Use API domain for this call
        QString apiBase = isSandbox ? "https://apisandbox.dev.clover.com" : "https://api.clover.com";
        QUrl mUrl(apiBase + "/v3/merchants/current");
        QNetworkRequest mReq(mUrl);
        mReq.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
        mReq.setRawHeader("Accept", "application/json");
        
        QNetworkReply *mReply = manager.get(mReq);
        QEventLoop mLoop;
        QObject::connect(mReply, &QNetworkReply::finished, &mLoop, &QEventLoop::quit);
        mLoop.exec();
        
        if (mReply->error() == QNetworkReply::NoError) {
            QJsonObject mRoot = QJsonDocument::fromJson(mReply->readAll()).object();
            mId = mRoot["id"].toString();
        } else {
             qDebug() << "Merchant ID Fetch Error:" << mReply->errorString();
        }
        mReply->deleteLater();
    }
    
    return {token.toStdString(), mId.toStdString()};
}

