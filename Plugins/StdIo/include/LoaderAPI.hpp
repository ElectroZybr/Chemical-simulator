#pragma once

class Document;

class LoaderAPI {
public:
    virtual void load(const Document& doc) = 0;
};