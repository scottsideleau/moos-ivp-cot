/****************************************************************************/
/* CotReporter.cpp */
/****************************************************************************/

// System Includes
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <queue>

// XML Includes
#include <rapidxml.hpp>
#include <rapidxml_print.hpp>

// ASIO Includes
#include <asio.hpp>

// CotReporter Includes
#include "CotReporter.h"


/*************************************************************************//**
 * A constructor for the CotReporter object.
 */
CMOOSCotReporter::CMOOSCotReporter( asio::io_context& io_context )
  : socket( io_context )
{   
  // Initialization
  bConnected = false;
  bNodeReport = false;
  bNodeReportLocal = false;

  // Character classification and case convention reset to POSIX standard
  setlocale(LC_CTYPE, "");
}


/*************************************************************************//**
 * A destructor for the CotReporter object.
 */
CMOOSCotReporter::~CMOOSCotReporter()
{
  sNodeReportQueue = {};
  sCotMessageQueue = {};
  socket.close();
  socket.release();
}


/*************************************************************************//**
 * Generate the timestamp in CCYY-MM-DDThh:mm:ss.ssZ format.
 */
void CMOOSCotReporter::GenerateZuluTimes(
    std::stringstream& ssTimeNowZ, std::stringstream& ssStaleTimeZ )
{
  // Get the current time in UTC (Coordinated Universal Time)
  std::chrono::system_clock::time_point now = 
    std::chrono::system_clock::now();

  // Convert the time point to a time_t (time since epoch)
  std::time_t time = std::chrono::system_clock::to_time_t( now );

  // Use std::put_time to format the time as a string
  std::tm tm = *std::gmtime( &time );  // Convert to UTC time
  ssTimeNowZ << std::put_time( &tm, "%Y-%m-%dT%H:%M:%S" )
    << ".";

  // Get the milliseconds
  auto milliseconds = 
    std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch() ) % 1000;

  // Add milliseconds to the formatted time
  ssTimeNowZ << std::to_string( milliseconds.count() );

  // Add the 'Z' for Zulu time (UTC)
  ssTimeNowZ << "Z";

  // Generate the 'stale' time
  std::chrono::system_clock::time_point staleNow = 
    now + std::chrono::seconds( 120 );
  std::time_t staleTime = std::chrono::system_clock::to_time_t( staleNow );
  std::tm staleTm = *std::gmtime( &staleTime );
  ssStaleTimeZ << std::put_time( &staleTm, "%Y-%m-%dT%H:%M:%S" )
    << ".";
  ssStaleTimeZ << std::to_string( milliseconds.count() );
  ssStaleTimeZ << "Z";

  /* Success */
}


/*************************************************************************//**
 * Generate the CoT Event message.
 */
std::string CMOOSCotReporter::GenerateEventMessage(
    std::string sTimeNowZ, std::string sStaleTimeZ, std::string sTakUid,
    std::string sLatitude, std::string sLongitude )
{
  // Create an XML document
  rapidxml::xml_document<> doc;

  // Add the XML version declaration
  rapidxml::xml_node<>* declaration = doc.allocate_node(
      rapidxml::node_type::node_declaration );
  declaration->append_attribute(
      doc.allocate_attribute("version", "1.0") );
  declaration->append_attribute(
      doc.allocate_attribute("standalone", "yes") );
  doc.append_node(declaration);

  // Add the root element
  rapidxml::xml_node<>* root = doc.allocate_node(
      rapidxml::node_type::node_element, "event" );
  doc.append_node( root );

  // Add attributes to the root element
  root->append_attribute(
      doc.allocate_attribute("version", "2.0") );
  //TODO: Handle UxV type
  root->append_attribute(
      doc.allocate_attribute("type", "a-G") );
  //TODO: UID
  root->append_attribute(
      doc.allocate_attribute("uid", sTakUid.c_str() ) );
  root->append_attribute(
      doc.allocate_attribute("how", "m-g") );
  //TODO: Time Now
  root->append_attribute(
      doc.allocate_attribute("time", sTimeNowZ.c_str() ) );
  root->append_attribute(
      doc.allocate_attribute("start", sTimeNowZ.c_str() ) );
  root->append_attribute(
      doc.allocate_attribute("stale", sStaleTimeZ.c_str() ) );

  // Add the <point> element
  rapidxml::xml_node<>* point = doc.allocate_node(
      rapidxml::node_type::node_element, "point" );
  root->append_node( point );

  // Add attributes to the <point> element
  point->append_attribute(
      doc.allocate_attribute("lat", sLatitude.c_str() ) );
  point->append_attribute(
      doc.allocate_attribute("lon", sLongitude.c_str() ) );
  point->append_attribute(
      doc.allocate_attribute("hae", "0") );
  point->append_attribute(
      doc.allocate_attribute("ce", "5") );
  point->append_attribute(
      doc.allocate_attribute("le", "5") );

  // Serialize the XML document to a string
  std::string sXmlMessage;
  rapidxml::print( std::back_inserter( sXmlMessage ), doc );

  /* Success */
  return sXmlMessage;
}


/*************************************************************************//**
 * Parse the NODE_REPORT(_LOCAL) message.
 */
void CMOOSCotReporter::ParseNodeReport( std::string sNodeReport,
    std::map< std::string, std::string >& mNodeReport )
{
  std::istringstream ssNodeReport( sNodeReport );
  std::string token;

  while ( std::getline( ssNodeReport, token, ',' ) )
  {
    std::istringstream ssToken( token );
    std::string key, value;

    if ( std::getline ( ssToken, key, '=' ) 
        && std::getline ( ssToken, value ) )
    {
      mNodeReport[ key ] = value;
    }
  }
  
  /* Success */
}


/*************************************************************************//**
 * Overloaded function that is run once, when CotReporter first connects
 * to the MOOSdb.  Registration for MOOSdb variables we want to track takes 
 * place here and in OnStartUp(), as per the MOOS documentation.
 *
 * @return  a boolean indicating success or failure
 */
bool CMOOSCotReporter::OnConnectToServer()
{
  DoRegistrations();

  /* Success */
  return true;
}


/*************************************************************************//**
 * Overloaded function called each time a MOOSdb variable that we are
 * subscribed to is updated.
 *
 * @return  a boolean indicating success or failure
 */
bool CMOOSCotReporter::OnNewMail(MOOSMSG_LIST &NewMail)
{
  MOOSMSG_LIST::iterator p;
  dfTimeNow = MOOSTime();

  // Process the new mail
  for(p = NewMail.begin(); p != NewMail.end(); p++)
  {
    CMOOSMsg &Message = *p;
    if (Message.m_sKey == "NODE_REPORT_LOCAL")
    {
      sNodeReportLocal = Message.m_sVal;      
      bNodeReportLocal = true;
    }

    if (Message.m_sKey == "NODE_REPORT")
    {
      sNodeReport = Message.m_sVal;
      sNodeReportQueue.push( sNodeReport );
      bNodeReport = true;
    }
  }

  /* Success */
  return true;
}                                      


/*************************************************************************//**
 * Overloaded function called every 1/Apptick to process data and do work.
 */
bool CMOOSCotReporter::Iterate()
{   
  // Get UTC Times
  std::stringstream ssTimeNowZ;
  std::stringstream ssStaleTimeZ;
  GenerateZuluTimes( ssTimeNowZ, ssStaleTimeZ );
  std::cerr << std::endl
    << "Time: " 
    << ssTimeNowZ.str()
    << std::endl;

  // Parse Local Node Report
  if ( bNodeReportLocal )
  {
    std::map< std::string, std::string > mNodeReportLocal;
    ParseNodeReport( sNodeReportLocal, mNodeReportLocal );

    // Extract relevant fields
    std::string sName = mNodeReportLocal[ "NAME" ];
    std::string sType = mNodeReportLocal[ "TYPE" ];
    std::string sTime = mNodeReportLocal[ "TIME" ];
    std::string sLatitude = mNodeReportLocal[ "LAT" ];
    std::string sLongitude = mNodeReportLocal[ "LON" ];
    std::string sDepth = mNodeReportLocal[ "DEP" ];
    std::string sMode = mNodeReportLocal[ "MODE" ];

    // Set Local UID
    std::stringstream ssTakUid;
    ssTakUid << sTakUidBase
      << "_" << sType
      << "_" << sName; 

    // Make Local CoT PLI Message
    std::string sCotMessage = GenerateEventMessage(
        ssTimeNowZ.str(), ssStaleTimeZ.str(), ssTakUid.str(),
        sLatitude, sLongitude );
    sCotMessageQueue.push( sCotMessage );
    /*std::cerr <<  sCotMessage
      << std::flush;*/

    mNodeReportLocal.clear();
    bNodeReportLocal = false;
  }

  // Parse Other Node Reports
  if ( bNodeReport )
  {
    while ( !sNodeReportQueue.empty() )
    {
      std::map< std::string, std::string > mNodeReport;
      ParseNodeReport( sNodeReportQueue.front(), mNodeReport ); 

      // Extract relevant fields
      std::string sName = mNodeReport[ "NAME" ];
      std::string sType = mNodeReport[ "TYPE" ];
      std::string sTime = mNodeReport[ "TIME" ];
      std::string sLatitude = mNodeReport[ "LAT" ];
      std::string sLongitude = mNodeReport[ "LON" ];
      std::string sDepth = mNodeReport[ "DEP" ];
      std::string sMode = mNodeReport[ "MODE" ];

      // Set UID
      std::stringstream ssTakUid;
      ssTakUid << sTakUidBase
        << "_" << sType
        << "_" << sName; 

      // Make CoT PLI Message
      std::string sCotMessage = GenerateEventMessage(
          ssTimeNowZ.str(), ssStaleTimeZ.str(), ssTakUid.str(),
          sLatitude, sLongitude );
      sCotMessageQueue.push( sCotMessage );
      /*std::cerr <<  sCotMessage
        << std::flush;*/

      mNodeReport.clear();
    }

    bNodeReport = false;
  }

  if ( bConnected )
  {
    // Send to TAK Server
    try 
    {
      while ( !sCotMessageQueue.empty() )
      {
        const std::string sToSend = sCotMessageQueue.front();
        asio::write(socket, asio::buffer( sToSend ) );
        sCotMessageQueue.pop();
        std::cerr << "Sent: "
          << sToSend
          << std::endl;
      }
    } 
    catch ( std::exception& e ) 
    {
      std::cerr << "Exception: " 
        << e.what() 
        << std::endl;
      bConnected = false;
      socket.close();
    }
  }
  else
  {
    // Connect to TAK Server
    endpoint.address( asio::ip::address::from_string( sTakServer ) );
    endpoint.port( static_cast< unsigned short >( nTakPort ) );
    try
    {
      socket.connect( endpoint );
      bConnected = true;
      std::cerr << "Connected to TAK Server: "
        << sTakServer
        << std::endl;
    }
    catch ( std::exception& e ) 
    {
      std::cerr << "Exception: " 
        << e.what() 
        << std::endl;
      bConnected = false;
      socket.close();
    }
  }

  /* Success */
  return true;        
}


/*************************************************************************//**
 * Overloaded function where we initialize the Local UTM coordinate system,
 * perform initializations based on the contents of the .moos file, and 
 * subscribe to any relevant MOOSdb variables.
 *
 * @return  a boolean indicating success or failure
 */
bool CMOOSCotReporter::OnStartUp()
{
  // Useful temporary variables
  std::string sVal;

  // Retrieve application-specific configuration parameters
  if(m_MissionReader.GetConfigurationParam("tak_server", sVal))
  {
    sTakServer = sVal;
  }

  if(m_MissionReader.GetConfigurationParam("tak_port", sVal))
  {
    nTakPort = std::stoul( sVal );
  }

  if(m_MissionReader.GetConfigurationParam("tak_uid_base", sVal))
  {
    sTakUidBase = sVal;
  }

  // Register for relevant MOOSdb variables
  DoRegistrations(); 

  /* Success */
  return true;
}


/*************************************************************************//**
 * Register for the variables of interest in the MOOSdb.
 */
void CMOOSCotReporter::DoRegistrations()
{   
  m_Comms.Register("NODE_REPORT", 0);
  m_Comms.Register("NODE_REPORT_LOCAL", 0);
}

