#include "simplepbf.hpp"
#include <iostream>


uint64 readUVarint(const std::string& data, size_t& pos) {
    uint64 result = 0;
    int count = 0;
    uint8_t b=0;

    do {
        if (count == 10) return 0;
        b = data[(pos)++];
        
        result |= (static_cast<uint64_t>(b & 0x7F) << (7 * count));
    
        ++count;
    } while (b & 0x80);
    
    return result;
}


    
int64 unZigZag(uint64 uv) {
    int64 x = (int64) (uv>>1);
    if ((uv&1)!=0) {
        x = x^(-1);
    }
    return x;
}

int64 readVarint(const std::string& data, size_t& pos) {
    uint64_t uv = readUVarint(data,pos);
    return unZigZag(uv);
}

std::string readData(const std::string& data, size_t& pos) {
    size_t uv = readUVarint(data,pos);
    //std::cout << "readData(..."<<*pos<<")=" << uv << std::endl;
    size_t p = pos;
    pos += uv;
    return data.substr(p,uv);
}

double toDouble(uint64 u) {
    double f = *(double *)(&u);
    return f;
}


boost::tuple<uint64, uint64, std::string> readPbfTag(const std::string& data, size_t& pos) {
    if ((pos) == data.size()) {
        return boost::make_tuple(0,0,"");
    }
    //std::cout << "tag @ " << *pos;
    uint64_t tag = readUVarint(data,pos);
    //std::cout << " " << tag << "["<<*pos<<"]"<< std::endl;
    if ((tag&7) == 0) {
        return boost::make_tuple(tag>>3, readUVarint(data,pos), "");
    } else if ((tag&7)==2) {
        return boost::make_tuple(tag>>3, 0, readData(data,pos));
    }
    std::cout << "?? @ " << pos << "/" << data.size() << ": " << tag << " "  << (tag&7) << " " << (tag>>3) << std::endl;
    
    throw std::domain_error("only understand varint & data");
    return boost::make_tuple(0,0,"");
}


std::vector<int64> readPackedDelta(const std::string& data) {
    size_t pos = 0;
    std::vector<int64> ans;
    
    int64 curr=0;
    
    while (pos<data.size()) {
        curr += readVarint(data,pos);
        ans.push_back(curr);
    }
    return ans;
}


std::vector<uint64> readPackedInt(const std::string& data)
{
    size_t pos = 0;
    std::vector<uint64> ans;
    
    while (pos<data.size()) {
        ans.push_back(readUVarint(data,pos));
    }
    return ans;
}


