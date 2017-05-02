// -----------------------------------------
// JetScape (modular/task) based framework
// Intial Design: Joern Putschke (2017)
//                (Wayne State University)
// -----------------------------------------
// License and Doxygen-like Documentation to be added ...

#ifndef JETSCAPEREADER_H
#define JETSCAPEREADER_H

#include "GTL/graph.h"
#include <GTL/edge_map.h>
#include <GTL/node_map.h>
#include "JetClass.hpp"
#include "JetScapeLogger.h"
#include "StringTokenizer.h"
#include "PartonShower.h"
#include <fstream>
#include "gzstream.h"

template<class T>
class JetScapeReader
{

 public:

  JetScapeReader();
  JetScapeReader(string m_file_name_in) {file_name_in =  m_file_name_in; Init();}
  virtual ~JetScapeReader();

  void Close() {inFile.close();}
  void Clear();
  
  void Next();
  bool Finished() {return inFile.eof();}
  
  int GetCurrentEvent() {return currentEvent-1;}
  int GetCurrentNumberOfPartonShowers() {return pShowers.size();}
  
  //shared_ptr<PartonShower> GetPartonShower() {return pShower;}
  vector<shared_ptr<PartonShower>> GetPartonShowers() {return pShowers;}
  
 private:

  StringTokenizer strT;
   
  void Init();
  void AddNode(string s);
  void AddEdge(string s);
  //void MakeGraph();
  
  string file_name_in;
  T inFile;
  
  int currentEvent;
  int currentShower;
  
  shared_ptr<PartonShower> pShower;
  vector<shared_ptr<PartonShower>> pShowers;
  
  vector<node> nodeVec;
  vector<edge> edgeVec; 
  
 };
#endif

// ---------------------

class JetScapeReaderAscii : public JetScapeReader<ifstream>
{

 public :
  
 JetScapeReaderAscii() : JetScapeReader() {};
 JetScapeReaderAscii(string m_file_name_in) : JetScapeReader(m_file_name_in) {};
  ~JetScapeReaderAscii() {};
  
};

class JetScapeReaderAsciiGZ : public JetScapeReader<igzstream>
{

 public :
  
 JetScapeReaderAsciiGZ() : JetScapeReader() {};
 JetScapeReaderAsciiGZ(string m_file_name_in) : JetScapeReader(m_file_name_in) {};
  ~JetScapeReaderAsciiGZ() {};
  
};

// ---------------------