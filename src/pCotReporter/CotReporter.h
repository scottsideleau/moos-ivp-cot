/****************************************************************************/
/* CotReporter.h */
/****************************************************************************/

#ifndef MOOS_APP_CotReporter_H_
#define MOOS_APP_CotReporter_H_

// System Includes
#include <queue>

// ASIO Includes
#include "asio.hpp"

// MOOS Includes
#include "MOOS/libMOOS/App/MOOSApp.h"
//#include "MOOS/libMOOSGeodesy/MOOSGeodesy.h"


/*************************************************************************//**
 * Class that extends CMOOSApp to produce pCotReporter.
 */
class CMOOSCotReporter : public CMOOSApp
{
  public:
    CMOOSCotReporter( asio::io_context& io_context );
    virtual ~CMOOSCotReporter();

  protected:
    // Inherited from MOOSApp
    bool OnConnectToServer();               /* overloaded */
    bool Iterate();                         /* overloaded */
    bool OnNewMail(MOOSMSG_LIST &NewMail);  /* overloaded */
    bool OnStartUp();                       /* overloaded */
    //bool MakeStatusString();              /* using default */
    //bool OnCommandMsg(CMOOSMsg CmdMsg);   /* unused */
    //bool ConfigureComms();                /* unused*/
    //bool OnDisconnectFromServer();        /* unused*/

  private:
    // Initialize the MOOS coordinate conversion utility
    //CMOOSGeodesy m_Geodesy;

    // Global variables filled by the .moos file
    std::string sTakServer;
    unsigned int nTakPort;
    std::string sTakUidBase;

    // Global variables
    //double dfLatOrigin;
    //double dfLonOrigin;
		bool bConnected;
		asio::ip::tcp::socket socket;
		asio::ip::tcp::endpoint endpoint;
    double dfTimeNow;
		std::string sNodeReportLocal;
		bool bNodeReportLocal;
		std::string sNodeReport;
		bool bNodeReport;
		std::queue< std::string > sNodeReportQueue;
		std::queue< std::string > sCotMessageQueue;

    // Functions
    void DoRegistrations();
    void GenerateZuluTimes( std::stringstream& ssTimeNowZ, 
        std::stringstream& ssStaleTimeZ );
    std::string GenerateEventMessage( std::string sTimeNowZ,
        std::string sStaleTimeZ, std::string sTakUid,
				std::string sLatitude, std::string sLongitude );
		void ParseNodeReport( std::string sNodeReport,
				std::map< std::string, std::string >& mNodeReport );
};

#endif // #ifndef MOOS_APP_CotReporter_H_

