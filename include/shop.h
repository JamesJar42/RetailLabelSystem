#pragma once
#include "product.h"
#include <vector>
#include <unordered_map>
#include <string>

struct CSVMapping {
    int barcodeCol = 0;
    int nameCol = 1;
    int priceCol = 2;
    // sizeCol removed
    int originalPriceCol = 3;
    int labelQuantityCol = 4;
    bool hasHeader = false; // If true, skip first line
};

class shop
{

    std::vector<product> database;
    std::unordered_map<std::string, size_t> barcodeIndex;
    std::string lastOAuthError;
    std::string lastOAuthRefreshToken;
    int lastOAuthExpiresIn = 0;

public:

    std::vector<product> &listProduct();

    // Adds a product to the store. Throws std::invalid_argument for empty/duplicate barcodes.
    void add(product pd);

    product& search(int choice);

    void removeByBarcode(const std::string &barcode);

    product& searchByBarcode(const std::string &barcode);

    void clear();
    
    // CSV Serialization
    // Format: Defined by mapping (default: Barcode,Name,Price,Size,WasPrice,LabelFlag)
    bool loadFromCSV(const std::string &path, const CSVMapping &mapping = CSVMapping());
    bool saveToCSV(const std::string &path);
    
    // Clover API Integration
    bool loadFromClover(const std::string &merchantId, const std::string &apiToken, bool isSandbox);
    bool validateCloverConnection(const std::string &merchantId, const std::string &apiToken, bool isSandbox);
    // Exchange OAuth code for token. Returns pair {access_token, merchant_id} or empty pair on failure.
    // For desktop/native flows, provide codeVerifier for PKCE and leave clientSecret empty.
    std::pair<std::string, std::string> exchangeCloverAuthCode(
        const std::string &clientId,
        const std::string &clientSecret,
        const std::string &code,
        bool isSandbox,
        const std::string &codeVerifier = "");
    // Refresh an OAuth access token. Returns pair {access_token, refresh_token_or_empty}.
    std::pair<std::string, std::string> refreshCloverAccessToken(
        const std::string &clientId,
        const std::string &clientSecret,
        const std::string &refreshToken,
        bool isSandbox);
    std::string getLastOAuthError() const;
    std::string getLastOAuthRefreshToken() const;
    int getLastOAuthExpiresIn() const;

};
