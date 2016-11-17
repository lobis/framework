///______________________________________________________________________________
///______________________________________________________________________________
///______________________________________________________________________________
///             
///
///             RESTSoft : Software for Rare Event Searches with TPCs
///
///             TRestMetadata.h
///
///             Base class from which to inherit all other REST metadata classes (config, etc...) 
///             TRestMetadata controls the access to config files.
///
///             jun 2014:   First concept
///                 Created as part of the conceptualization of existing REST 
///                 software.
///                 Igor G. Irastorza
///
///              Jul 2015:    Implementation  
///                 J. Galan
///_______________________________________________________________________________


#ifndef RestCore_TRestMetadata
#define RestCore_TRestMetadata

#include <stdio.h>
#include <stdlib.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream> 
#include <algorithm> 
#include <sys/stat.h>


#include <TFormula.h>
#include <TVector2.h>
#include <TVector3.h>
#include <TNamed.h>
#include "string.h"

#include <TRestSystemOfUnits.h>

const int PARAMETER_NOT_FOUND_INT = -99999999;
const double PARAMETER_NOT_FOUND_DBL = -99999999;
const TString PARAMETER_NOT_FOUND_STR = "-99999999";

enum REST_Verbose_Level {REST_Silent, REST_Warning, REST_Info, REST_Debug };


class TRestMetadata:public TNamed {

    protected:
        std::string GetFieldValue( std::string fieldName, std::string definition, size_t fromPosition = 0 );
        Double_t GetDblFieldValueWithUnits( string fieldName, string definition, size_t fromPosition = 0 );
        TVector2 Get2DVectorFieldValueWithUnits( string fieldName, string definition, size_t fromPosition = 0 );
        TVector3 Get3DVectorFieldValueWithUnits( string fieldName, string definition, size_t fromPosition = 0 );

        std::string GetKEYStructure( std::string keyName );
        std::string GetKEYStructure( std::string keyName, std::string buffer );
        std::string GetKEYStructure( std::string keyName, size_t &fromPosition );
        std::string GetKEYStructure( std::string keyName, size_t &fromPosition, std::string buffer );

        std::string GetKEYDefinition( std::string keyName );
        std::string GetKEYDefinition( std::string keyName, std::string buffer );
        std::string GetKEYDefinition( std::string keyName, size_t &fromPosition, std::string buffer );
        std::string GetKEYDefinition( std::string keyName, size_t &fromPosition );

        std::string GetParameter( std::string parName, size_t &pos, std::string inputString );
        Double_t GetDblParameterWithUnits( std::string parName, size_t &pos, std::string inputString );
        TVector2 Get2DVectorParameterWithUnits( std::string parName, size_t &pos, std::string inputString );
        TVector3 Get3DVectorParameterWithUnits( std::string parName, size_t &pos, std::string inputString );

        vector <string> GetObservablesList( );
        vector <string> GetObservableDescriptionsList( );

        std::string GetMyParameter( std::string &value, size_t &pos );

        std::string fConfigFileName;		// std::string with the name of the config file
        std::string fSectionName;        // section name given in the constructor of TRestSpecificMetadata

        // This method must be implemented in the derived class to fill the class fields with a given section
        virtual void InitFromConfigFile( ) = 0; 

        virtual void Initialize() = 0;// { cout << __PRETTY_FUNCTION__ << endl; };

        Int_t LoadSectionMetadata( std::string section, std::string cfgFileName, string name="" );

        Int_t LoadConfigFromFile( string cfgFileName, std::string name );
        Int_t LoadConfigFromFile( std::string cfgFileName );

    private:

        ifstream configFile;    //! pointer to config file to load metadata
        std::string configBuffer;

        std::string fConfigFilePath;		//  std::string with the path to the config file
        
        TString fDataPath;
        REST_Verbose_Level fVerboseLevel;

#ifndef __CINT__
        Bool_t fStore;

#endif
        
        void SetDefaultConfigFilePath();

        void SetConfigFile( std::string cfgFileName );

        Int_t CheckConfigFile( );

        void SetEnvVariable( size_t &pos );
        std::string GetFieldValue( std::string fieldName, size_t fromPosition );
        std::string GetUnits( string definition, size_t fromPosition );
        std::string GetFieldFromKEY( std::string parName, std::string key );

        std::string EvaluateExpression( std::string exp );

        std::string ExpandForLoops( std::string buffer );
        std::string ReplaceMathematicalExpressions( std::string buffer );
        std::string ReplaceEnvironmentalVariables( const std::string buffer );
        std::string ExtractLoopStructure( std::string in, size_t pos );
        std::string RemoveComments( string in );

        std::string GetSectionByNameFromFile( std::string nref, std::string fref );
        Int_t FindSection( std::string buffer, size_t startPos = 0 );

    public:
        void CheckSection( );

        void SetConfigFilePath(const char *configFilePath);

        TString GetDataPath( ) { return fDataPath; }

        void SetDataPath( TString path ) { fDataPath = path; }

        REST_Verbose_Level GetVerboseLevel( ) { return fVerboseLevel; }

        Int_t FindEndSection( Int_t initPos );

        TString GetMainDataPath() { return fDataPath; }

        std::string GetParameter( std::string parName, TString defaultValue = PARAMETER_NOT_FOUND_STR );
        Double_t GetDblParameterWithUnits( std::string parName, Double_t defaultValue = PARAMETER_NOT_FOUND_DBL );
        TVector2 Get2DVectorParameterWithUnits( string parName, TVector2 defaultValue = TVector2(-1,-1) );
        TVector3 Get3DVectorParameterWithUnits( string parName, TVector3 defaultValue = TVector3( -1, -1, -1) );

        void PrintTimeStamp( Double_t timeStamp );
        void PrintConfigBuffer( );
        
        // String helper classes. Declared static to be able to access them without having to instantiate TRestMetadata.
        // Probably they should be in a different class (i.e. TRestStrings)
        static Int_t isANumber( std::string in );
        Int_t isAExpression( std::string in );
        static std::string trim(std::string str);
        static Double_t StringToDouble( std::string in );
        static Int_t StringToInteger( std::string in );
        static TVector3 StringTo3DVector( std::string in );
        static TVector2 StringTo2DVector( std::string in );
        static std::string RemoveWhiteSpaces( std::string in );
        static std::string Replace( std::string in, std::string thisString, std::string byThisString, size_t fromPosition );
        static Int_t Count( std::string s, std::string sbstring);
        static bool fileExists( const std::string& filename );
        static bool isRootFile( const std::string& filename ); 
        static void GetChar(){ cout << "Press a KEY to continue ..." << endl; getchar(); }

        void DoNotStore( ) { fStore = false; }
        Bool_t Store( ) { return fStore; }


        //////////////////////////////////////////////////

        void virtual PrintMetadata() = 0;

        //Constructor
        TRestMetadata();
        TRestMetadata( const char *cfgFileName);
        //Destructor
        ~TRestMetadata();

        /*
           std::string SearchString(const char *name);
           Int_t SearchInt(const char *name);
           Double_t SearchDouble(const char *name);
           */

        ClassDef(TRestMetadata, 1); // Rest metadata Base class 
};
#endif
