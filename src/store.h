#pragma once
#ifndef STORE_H
#define STORE_H

#include <string>

namespace MagicTower
{

/* 
CREATE TABLE stores (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    usability BOOLEAN DEFAULT (1),
    name      TEXT,
    content   TEXT
); 
*/
struct Store
{
    bool usability;
    std::string name;
    std::string content;
};

}

#endif
