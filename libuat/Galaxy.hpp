#ifndef __GALAXY_HPP__
#define __GALAXY_HPP__

#include <string>

#include "../common/HashSearchBig.hpp"
#include "../common/MetaView.hpp"

class Galaxy
{
public:
    static Galaxy* Open( const std::string& fn );

private:
    Galaxy( const std::string& dir );

    std::string m_base;
    const MetaView<uint64_t, char> m_middb;
    const HashSearchBig m_midhash;
};

#endif
