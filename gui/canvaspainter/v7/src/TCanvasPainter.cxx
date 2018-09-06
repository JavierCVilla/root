/// \file TCanvasPainter.cxx
/// \ingroup CanvasPainter ROOT7
/// \author Axel Naumann <axel@cern.ch>
/// \date 2017-05-31
/// \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
/// is welcome!

/*************************************************************************
 * Copyright (C) 1995-2017, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "ROOT/RVirtualCanvasPainter.hxx"
#include "ROOT/RCanvas.hxx"
#include <ROOT/TLogger.hxx>
#include <ROOT/RDisplayItem.hxx>
#include <ROOT/RPadDisplayItem.hxx>
#include <ROOT/RMenuItem.hxx>

#include <ROOT/TWebWindow.hxx>
#include <ROOT/TWebWindowsManager.hxx>

#include <memory>
#include <string>
#include <vector>
#include <list>
#include <thread>
#include <chrono>
#include <fstream>

#include "TList.h"
#include "TROOT.h"
#include "TClass.h"
#include "TBufferJSON.h"
#include "TBase64.h"

// ==========================================================================================================

// new implementation of canvas painter, using TWebWindow

namespace ROOT {
namespace Experimental {

class TCanvasPainter : public Internal::RVirtualCanvasPainter {
private:
   struct WebConn {
      unsigned fConnId{0};    ///<! connection id
      bool fDrawReady{false}; ///!< when first drawing is performed
      std::string fGetMenu;   ///<! object id for menu request
      uint64_t fSend{0};      ///<! indicates version send to connection
      uint64_t fDelivered{0}; ///<! indicates version confirmed from canvas
      WebConn() = default;
      WebConn(unsigned connid) : fConnId(connid) {}
   };

   struct WebCommand {
      std::string fId;                     ///<! command identifier
      std::string fName;                   ///<! command name
      std::string fArg;                    ///<! command arguments
      bool fRunning{false};                ///<! true when command submitted
      bool fReady{false};                  ///<! true when command finished
      bool fResult{false};                 ///<! result of command execution
      CanvasCallback_t fCallback{nullptr}; ///<! callback function associated with command
      unsigned fConnId{0};                 ///<! connection for the command - 0 any can be used
      WebCommand() = default;
      WebCommand(const std::string &id, const std::string &name, const std::string &arg, CanvasCallback_t callback,
                 unsigned connid)
         : fId(id), fName(name), fArg(arg), fCallback(callback), fConnId(connid)
      {
      }
      void CallBack(bool res)
      {
         if (fCallback)
            fCallback(res);
         fCallback = nullptr;
      }
   };

   struct WebUpdate {
      uint64_t fVersion{0};                ///<! canvas version
      CanvasCallback_t fCallback{nullptr}; ///<! callback function associated with the update
      WebUpdate() = default;
      WebUpdate(uint64_t ver, CanvasCallback_t callback) : fVersion(ver), fCallback(callback) {}
      void CallBack(bool res)
      {
         if (fCallback) fCallback(res);
         fCallback = nullptr;
      }
   };

   typedef std::vector<ROOT::Experimental::Detail::RMenuItem> MenuItemsVector;

   /// The canvas we are painting. It might go out of existence while painting.
   const RCanvas &fCanvas; ///<!  Canvas

   std::shared_ptr<TWebWindow> fWindow; ///!< configured display

   std::list<WebConn> fWebConn; ///<! connections list
   bool fHadWebConn{false};     ///<! true if any connection were existing
   std::list<std::shared_ptr<WebCommand>> fCmds; ///<! list of submitted commands
   uint64_t fCmdsCnt{0};        ///<! commands counter
   // RPadDisplayItem fDisplayList;   ///<! full list of items to display
   // std::string fCurrentDrawableId; ///<! id of drawable, which paint method is called

   uint64_t fSnapshotVersion{0};     ///<! version of snapshot
   std::string fSnapshot;            ///<! last produced snapshot
   uint64_t fSnapshotDelivered{0};   ///<! minimal version delivered to all connections
   std::list<WebUpdate> fUpdatesLst; ///<! list of callbacks for canvas update

   std::string fNextDumpName;     ///<! next filename for dumping JSON

   /// Disable copy construction.
   TCanvasPainter(const TCanvasPainter &) = delete;

   /// Disable assignment.
   TCanvasPainter &operator=(const TCanvasPainter &) = delete;

   void CancelUpdates();

   void CancelCommands(unsigned connid = 0);

   void CheckDataToSend();

   void ProcessData(unsigned connid, const std::string &arg);

   std::string CreateSnapshot(const ROOT::Experimental::RCanvas &can);

   std::shared_ptr<RDrawable> FindDrawable(const ROOT::Experimental::RCanvas &can, const std::string &id);

   void CreateWindow();

   void SaveCreatedFile(std::string &reply);

   void FrontCommandReplied(const std::string &reply);

   int CheckDeliveredVersion(uint64_t ver, double);

public:
   TCanvasPainter(const RCanvas &canv) : fCanvas(canv) {}

   virtual ~TCanvasPainter();

   //   virtual void AddDisplayItem(std::unique_ptr<RDisplayItem> &&item) override
   //   {
   //      item->SetObjectID(fCurrentDrawableId);
   //      fDisplayList.Add(std::move(item));
   //   }

   virtual void CanvasUpdated(uint64_t ver, bool async, ROOT::Experimental::CanvasCallback_t callback) override;

   /// return true if canvas modified since last painting
   virtual bool IsCanvasModified(uint64_t id) const override { return fSnapshotDelivered != id; }

   /// perform special action when drawing is ready
   virtual void
   DoWhenReady(const std::string &name, const std::string &arg, bool async, CanvasCallback_t callback) override;

   virtual void NewDisplay(const std::string &where) override;

   virtual int NumDisplays() const override;

   virtual void Run(double tm = 0.) override;

   virtual bool AddPanel(std::shared_ptr<TWebWindow>) override;

   /** \class CanvasPainterGenerator
          Creates TCanvasPainter objects.
        */

   class GeneratorImpl : public Generator {
   public:
      /// Create a new TCanvasPainter to paint the given RCanvas.
      std::unique_ptr<RVirtualCanvasPainter> Create(const ROOT::Experimental::RCanvas &canv) const override
      {
         return std::make_unique<TCanvasPainter>(canv);
      }
      ~GeneratorImpl() = default;

      /// Set RVirtualCanvasPainter::fgGenerator to a new GeneratorImpl object.
      static void SetGlobalPainter()
      {
         if (GetGenerator()) {
            R__ERROR_HERE("CanvasPainter") << "Generator is already set! Skipping second initialization.";
            return;
         }
         GetGenerator().reset(new GeneratorImpl());
      }

      /// Release the GeneratorImpl object.
      static void ResetGlobalPainter() { GetGenerator().reset(); }
   };
};

struct TNewCanvasPainterReg {
   TNewCanvasPainterReg() { TCanvasPainter::GeneratorImpl::SetGlobalPainter(); }
   ~TNewCanvasPainterReg() { TCanvasPainter::GeneratorImpl::ResetGlobalPainter(); }
} newCanvasPainterReg;

} // namespace Experimental
} // namespace ROOT

/////////////////////////////////////////////////////////////////////////////////////////////
/// destructor

ROOT::Experimental::TCanvasPainter::~TCanvasPainter()
{
   CancelCommands();
   CancelUpdates();
   if (fWindow)
      fWindow->CloseConnections();
}

/////////////////////////////////////////////////////////////////////////////////////////////
/// Checks if specified version was delivered to all clients
/// Used to wait for such condition

int ROOT::Experimental::TCanvasPainter::CheckDeliveredVersion(uint64_t ver, double)
{
   if (fWebConn.empty() && fHadWebConn)
      return -1;
   if (fSnapshotDelivered >= ver)
      return 1;
   return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/// Cancel all pending Canvas::Update()

void ROOT::Experimental::TCanvasPainter::CancelUpdates()
{
   fSnapshotDelivered = 0;
   auto iter = fUpdatesLst.begin();
   while (iter != fUpdatesLst.end()) {
      auto curr = iter++;
      curr->fCallback(false);
      fUpdatesLst.erase(curr);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Cancel command execution on provided connection
/// All commands are cancelled, when connid === 0

void ROOT::Experimental::TCanvasPainter::CancelCommands(unsigned connid)
{
   auto iter = fCmds.begin();
   while (iter != fCmds.end()) {
      auto cmd = *iter;
      if (!connid || (cmd->fConnId == connid)) {
         cmd->CallBack(false);
         cmd->fReady = true;
         cmd->fRunning = false;
         fCmds.erase(iter++);
      } else {
         iter++;
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Check if canvas need to sand data to the clients

void ROOT::Experimental::TCanvasPainter::CheckDataToSend()
{
   uint64_t min_delivered = 0;

   for (auto &&conn : fWebConn) {

      if (conn.fDelivered && (!min_delivered || (min_delivered < conn.fDelivered)))
         min_delivered = conn.fDelivered;

      // check if direct data sending is possible
      if (!fWindow->CanSend(conn.fConnId, true))
         continue;

      TString buf;

      if (conn.fDrawReady && !fCmds.empty() && !fCmds.front()->fRunning &&
          ((fCmds.front()->fConnId == 0) || (fCmds.front()->fConnId == conn.fConnId))) {
         auto &cmd = fCmds.front();
         cmd->fRunning = true;
         cmd->fConnId = conn.fConnId;
         buf = "CMD:";
         buf.Append(cmd->fId);
         buf.Append(":");
         buf.Append(cmd->fName);
      } else if (!conn.fGetMenu.empty()) {
         auto drawable = FindDrawable(fCanvas, conn.fGetMenu);

         R__DEBUG_HERE("CanvasPainter") << "Request menu for object " << conn.fGetMenu;

         if (drawable) {

            ROOT::Experimental::RMenuItems items;

            drawable->PopulateMenu(items);

            // FIXME: got problem with std::list<RMenuItem>, can be generic TBufferJSON
            buf = "MENU:";
            buf.Append(conn.fGetMenu);
            buf.Append(":");
            buf.Append(items.ProduceJSON());
         }

         conn.fGetMenu = "";
      } else if (conn.fSend != fSnapshotVersion) {
         // buf = "JSON";
         // buf  += TBufferJSON::ConvertToJSON(Canvas(), 3);

         conn.fSend = fSnapshotVersion;
         buf = "SNAP:";
         buf += TString::ULLtoa(fSnapshotVersion, 10);
         buf += ":";
         buf += fSnapshot;
      }

      if (buf.Length() > 0) {
         // sending of data can be moved into separate thread - not to block user code
         fWindow->Send(conn.fConnId, buf.Data());
      }
   }

   // if there are updates submitted, but all connections disappeared - cancel all updates
   if (fWebConn.empty() && fSnapshotDelivered)
      return CancelUpdates();

   if (fSnapshotDelivered != min_delivered) {
      fSnapshotDelivered = min_delivered;

      auto iter = fUpdatesLst.begin();
      while (iter != fUpdatesLst.end()) {
         auto curr = iter;
         iter++;
         if (curr->fVersion <= fSnapshotDelivered) {
            curr->CallBack(true);
            fUpdatesLst.erase(curr);
         }
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Method invoked when canvas should be updated on the client side
/// Depending from delivered status, each client will received new data

void ROOT::Experimental::TCanvasPainter::CanvasUpdated(uint64_t ver, bool async,
                                                       ROOT::Experimental::CanvasCallback_t callback)
{
   if (fWindow)
      fWindow->Sync();

   if (ver && fSnapshotDelivered && (ver <= fSnapshotDelivered)) {
      // if given canvas version was already delivered to clients, can return immediately
      if (callback)
         callback(true);
      return;
   }

   fSnapshotVersion = ver;
   fSnapshot = CreateSnapshot(fCanvas);

   if (!fWindow || !fWindow->IsShown()) {
      if (callback)
         callback(false);
      return;
   }

   CheckDataToSend();

   if (callback)
      fUpdatesLst.emplace_back(ver, callback);

   // wait that canvas is painted
   if (!async)
      fWindow->WaitFor([this, ver](double tm) { return CheckDeliveredVersion(ver, tm); });
}

//////////////////////////////////////////////////////////////////////////
/// perform special action when drawing is ready

void ROOT::Experimental::TCanvasPainter::DoWhenReady(const std::string &name, const std::string &arg, bool async,
                                                     CanvasCallback_t callback)
{
   if (name == "JSON") {
      // it is only for debugging, JSON does not invoke callback
      fNextDumpName = arg;
      return;
   }

   CreateWindow();

   // create batch job to execute action
   unsigned connid = fWindow->MakeBatch();

   if (!connid) {
      if (callback)
         callback(false);
      return;
   }

   auto cmd = std::make_shared<WebCommand>(std::to_string(++fCmdsCnt), name, arg, callback, connid);
   fCmds.emplace_back(cmd);

   CheckDataToSend();

   if (async) return;

   int res = fWindow->WaitFor([this, cmd](double tm) {
      if (cmd->fReady) {
         R__DEBUG_HERE("CanvasPainter") << "Command " << cmd->fName << " done";
         return cmd->fResult ? 1 : -1;
      }

      // connection is gone
      if (!fWindow->HasConnection(cmd->fConnId, false))
         return -2;

      // timeout
      if (tm > 100.)
         return -3;

      return 0;
   });

   if (res <= 0)
      R__ERROR_HERE("CanvasPainter") << name << " fail with " << arg << " result = " << res;
}

//////////////////////////////////////////////////////////////////////////
/// Process data from the client

void ROOT::Experimental::TCanvasPainter::ProcessData(unsigned connid, const std::string &arg)
{
   if (arg == "CONN_READY") {
      // special argument from TWebWindow itself
      // indication that new connection appeared

      fWebConn.emplace_back(connid);
      fHadWebConn = true;

      CheckDataToSend();
      return;
   }

   WebConn *conn(nullptr);
   auto iter = fWebConn.begin();
   while (iter != fWebConn.end()) {
      if (iter->fConnId == connid) {
         conn = &(*iter);
         break;
      }
      ++iter;
   }

   if (!conn)
      return; // no connection found

   // R__DEBUG_HERE("CanvasPainter") << "from client " << connid << " got data len:" << arg.length() << " val:" <<
   // arg.substr(0,30);

   if (arg == "CONN_CLOSED") {
      // special argument from TWebWindow itself
      // connection is closed

      fWebConn.erase(iter);

      // if there are no other connections - cancel all submitted commands
      CancelCommands(connid);

   } else if (arg.find("READY") == 0) {

   } else if (arg.find("SNAPDONE:") == 0) {
      std::string cdata = arg;
      cdata.erase(0, 9);
      conn->fDrawReady = true;                       // at least first drawing is performed
      conn->fDelivered = (uint64_t)std::stoll(cdata); // delivered version of the snapshot
   } else if (arg.find("RREADY:") == 0) {
      conn->fDrawReady = true; // at least first drawing is performed
   } else if (arg.find("GETMENU:") == 0) {
      std::string cdata = arg;
      cdata.erase(0, 8);
      conn->fGetMenu = cdata;
   } else if (arg == "QUIT") {
      // use window manager to correctly terminate http server
      TWebWindowsManager::Instance()->Terminate();
      return;
   } else if (arg == "RELOAD") {
      conn->fSend = 0; // reset send version, causes new data sending
   } else if (arg == "INTERRUPT") {
      gROOT->SetInterrupt();
   } else if (arg.find("REPLY:") == 0) {
      std::string cdata = arg;
      cdata.erase(0, 6);
      const char *sid = cdata.c_str();
      const char *separ = strchr(sid, ':');
      std::string id;
      if (separ)
         id.append(sid, separ - sid);
      if (fCmds.empty()) {
         R__ERROR_HERE("CanvasPainter") << "Get REPLY without command";
      } else if (!fCmds.front()->fRunning) {
         R__ERROR_HERE("CanvasPainter") << "Front command is not running when get reply";
      } else if (fCmds.front()->fId != id) {
         R__ERROR_HERE("CanvasPainter") << "Mismatch with front command and ID in REPLY";
      } else {
         FrontCommandReplied(separ + 1);
      }
   } else if (arg.find("SAVE:") == 0) {
      std::string cdata = arg;
      cdata.erase(0, 5);
      SaveCreatedFile(cdata);
   } else if (arg.find("OBJEXEC:") == 0) {
      std::string cdata = arg;
      cdata.erase(0, 8);
      size_t pos = cdata.find(':');

      if ((pos != std::string::npos) && (pos > 0)) {
         std::string id(cdata, 0, pos);
         cdata.erase(0, pos + 1);
         auto drawable = FindDrawable(fCanvas, id);
         if (drawable && (cdata.length() > 0)) {
            R__DEBUG_HERE("CanvasPainter") << "execute " << cdata << " for drawable " << id;
            drawable->Execute(cdata);
         } else if (id == "canvas") {
            R__DEBUG_HERE("CanvasPainter") << "execute " << cdata << " for canvas itself (ignored)";
         }
      }
   } else {
      R__ERROR_HERE("CanvasPainter") << "Got not recognized reply" << arg;
   }

   CheckDataToSend();
}

void ROOT::Experimental::TCanvasPainter::CreateWindow()
{
   if (fWindow) return;

   fWindow = TWebWindowsManager::Instance()->CreateWindow();
   fWindow->SetConnLimit(0); // allow any number of connections
   fWindow->SetDefaultPage("file:$jsrootsys/files/canvas.htm");
   fWindow->SetDataCallBack([this](unsigned connid, const std::string &arg) { ProcessData(connid, arg); });
   // fWindow->SetGeometry(500,300);
}


//////////////////////////////////////////////////////////////////////////
/// Create new display for the canvas
/// See ROOT::Experimental::TWebWindowsManager::Show() docu for more info

void ROOT::Experimental::TCanvasPainter::NewDisplay(const std::string &where)
{
   CreateWindow();

   fWindow->Show(where);
}

//////////////////////////////////////////////////////////////////////////
/// Returns number of connected displays

int ROOT::Experimental::TCanvasPainter::NumDisplays() const
{
   if (!fWindow) return 0;

   return fWindow->NumConnections();
}


//////////////////////////////////////////////////////////////////////////
/// Add window as panel inside canvas window

bool ROOT::Experimental::TCanvasPainter::AddPanel(std::shared_ptr<TWebWindow> win)
{
   if (!fWindow) {
      R__ERROR_HERE("CanvasPainter") << "Canvas not yet shown in AddPanel";
      return false;
   }

   if (!fWindow->IsShown()) {
      R__ERROR_HERE("CanvasPainter") << "Canvas window was not shown to call AddPanel";
      return false;
   }

   std::string addr = fWindow->RelativeAddr(win);

   if (addr.length() == 0) {
      R__ERROR_HERE("CanvasPainter") << "Cannot attach panel to canvas";
      return false;
   }

   // connection is assigned, but can be refused by the client later
   // therefore handle may be removed later

   std::string cmd("ADDPANEL:");
   cmd.append(addr);

   /// one could use async mode
   DoWhenReady(cmd, "AddPanel", true, nullptr);

   return true;
}

////////////////////////////////////////////////////////////////////////////////
/// Create JSON representation of data, which should be send to the clients
/// Here server-side painting is performed - each drawable adds own elements in
/// so-called display list, which transferred to the clients

std::string ROOT::Experimental::TCanvasPainter::CreateSnapshot(const ROOT::Experimental::RCanvas &can)
{

   PaintDrawables(can);

   fPadDisplayItem->SetObjectID("canvas"); // for canvas itself use special id
   fPadDisplayItem->SetTitle(can.GetTitle());
   fPadDisplayItem->SetWindowSize(can.GetSize());

   TString res = TBufferJSON::ToJSON(fPadDisplayItem.get(), 23);

   if (!fNextDumpName.empty()) {
      TBufferJSON::ExportToFile(fNextDumpName.c_str(), fPadDisplayItem.get(),
         gROOT->GetClass("ROOT::Experimental::RPadDisplayItem"));
      fNextDumpName.clear();
   }

   fPadDisplayItem.reset(); // no need to keep memory any longer

   return std::string(res.Data());
}

////////////////////////////////////////////////////////////////////////////////
/// Find drawable in the canvas with specified id
/// Used to communicate with the clients, which does not have any pointer

std::shared_ptr<ROOT::Experimental::RDrawable>
ROOT::Experimental::TCanvasPainter::FindDrawable(const ROOT::Experimental::RCanvas &can, const std::string &id)
{
   std::string search = id;
   size_t pos = search.find("#");
   // exclude extra specifier, later can be used for menu and commands execution
   if (pos != std::string::npos)
      search.resize(pos);

   return can.FindDrawable(search);
}

////////////////////////////////////////////////////////////////////////////////
/// Method called when GUI sends file to save on local disk
/// File coded with base64 coding

void ROOT::Experimental::TCanvasPainter::SaveCreatedFile(std::string &reply)
{
   size_t pos = reply.find(":");
   if ((pos == std::string::npos) || (pos == 0)) {
      R__ERROR_HERE("CanvasPainter") << "SaveCreatedFile does not found ':' separator";
      return;
   }

   std::string fname(reply, 0, pos);
   reply.erase(0, pos + 1);

   TString binary = TBase64::Decode(reply.c_str());

   std::ofstream ofs(fname, std::ios::binary);
   ofs.write(binary.Data(), binary.Length());
   ofs.close();

   R__INFO_HERE("CanvasPainter") << " Save file from GUI " << fname << " len " << binary.Length();
}

////////////////////////////////////////////////////////////////////////////////
/// Process reply on the currently active command

void ROOT::Experimental::TCanvasPainter::FrontCommandReplied(const std::string &reply)
{
   auto cmd = fCmds.front();
   fCmds.pop_front();

   cmd->fReady = true;

   bool result = false;

   if ((cmd->fName == "SVG") || (cmd->fName == "PNG") || (cmd->fName == "JPEG")) {
      if (reply.length() == 0) {
         R__ERROR_HERE("CanvasPainter") << "Fail to produce image" << cmd->fArg;
      } else {
         TString content = TBase64::Decode(reply.c_str());
         std::ofstream ofs(cmd->fArg, std::ios::binary);
         ofs.write(content.Data(), content.Length());
         ofs.close();
         R__INFO_HERE("CanvasPainter") << cmd->fName << " create file " << cmd->fArg << " length " << content.Length();
         result = true;
      }
   } else if (cmd->fName.find("ADDPANEL:") == 0) {
      R__DEBUG_HERE("CanvasPainter") << "get reply for ADDPANEL " << reply;
      result = (reply == "true");
   } else {
      R__ERROR_HERE("CanvasPainter") << "Unknown command " << cmd->fName;
   }

   cmd->fResult = result;
   cmd->CallBack(result);
}

/////////////////////////////////////////////////////////////////////////////////////////////
/// Run canvas functionality for specified period of time
/// Required when canvas used not from the main thread

void ROOT::Experimental::TCanvasPainter::Run(double tm)
{
   if (fWindow) {
      fWindow->Run(tm);
   } else if (tm>0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(int(tm*1000)));
   }
}
