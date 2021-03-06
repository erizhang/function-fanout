// Copyright (c) 2012 Shi Yuanmin.Simon. All rights reserved.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Path.h"

#include "JSONFormatter.h"

using namespace clang;

namespace FunctionFanout {

class ASTVistor: public clang::ASTConsumer, public clang::RecursiveASTVisitor<ASTVistor>
{
private:
   CompilerInstance& CI_;
   llvm::raw_ostream& ost_;
   JSONFormatter& fmt_;
public:
   ASTVistor(CompilerInstance &CI, llvm::raw_ostream* ost, JSONFormatter* fmt) :
               CI_(CI), ost_(*ost), fmt_(*fmt)
   {
   }

   virtual ~ASTVistor()
   {
      //ost_ << __FUNCTION__ << "\n";
      fmt_.EndSourceFile();
   }

   // @override clang::ASTConsumer::
   virtual bool HandleTopLevelDecl(DeclGroupRef DG);

   // @override clang::RecursiveASTVisitor<>::
   virtual bool VisitCallExpr(clang::CallExpr* expr);
};

bool ASTVistor::HandleTopLevelDecl(DeclGroupRef DG)
{
   for (DeclGroupRef::iterator i = DG.begin(), e = DG.end(); i != e; ++i) {
      const Decl *D = *i;
      if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
         if (!FD->hasBody()) continue;
         if (CI_.getSourceManager().isInSystemHeader(FD->getLocation())) {
            //loc.print(llvm::errs(), CI_.getSourceManager());
            continue;
         }

         std::vector<std::string> params;
         for (FunctionDecl::param_const_iterator it = FD->param_begin(); it != FD->param_end(); ++it) {
            params.push_back((*it)->getOriginalType().getAsString());
         }

         fmt_.AddDefinition(FD->getQualifiedNameAsString(), FD->getResultType().getAsString(), params);

         this->TraverseStmt(FD->getBody());

         fmt_.EndDefinition();
      }
   }

   ost_.flush();

   return true;
}

bool ASTVistor::VisitCallExpr(clang::CallExpr* expr)
{
   const clang::FunctionDecl* FD = expr->getDirectCallee();
   if (!FD) {
      // Maybe a function pointer
//      const Decl *D = expr->getCalleeDecl();
//      const VarDecl* VD = dyn_cast<VarDecl>(D);
//      SourceLocation loc = expr->getLocStart();
//      bool inv;
//      unsigned ln = CI_.getSourceManager().getExpansionLineNumber(loc, &inv);
//      llvm::errs() << "not a FunctionDecl in CallExpr:{kind:" << D->getDeclKindName();
//      if (VD) {
//         llvm::errs() << ", name:" << VD->getQualifiedNameAsString();
//      }
//      llvm::errs() << ", ln:" << ln << "}\n";

      return true;
   }

   std::vector<std::string> params;
   for (FunctionDecl::param_const_iterator it = FD->param_begin(); it != FD->param_end(); ++it) {
      params.push_back((*it)->getOriginalType().getAsString());
   }
   fmt_.AddCallee(FD->getQualifiedNameAsString(), FD->getResultType().getAsString(), params);

   return true;
}

class ASTAction: public PluginASTAction
{
   llvm::raw_fd_ostream* output_;

   void CreateOutput(CompilerInstance& CI)
   {
      //llvm::errs() << __FUNCTION__ << ":" << CI.getFrontendOpts().OutputFile << "\n";
      if (output_) return;

      std::string errMsg;
      output_ = CI.createOutputFile("", errMsg, true, true, CI.getFrontendOpts().OutputFile, "fanout");
      if (!output_) llvm::errs() << "Failed to create output file:" << errMsg << "\n";
   }

public:
   ASTAction() :
               output_(0)
   {
   }
   virtual ~ASTAction()
   {
      //llvm::errs() << __FUNCTION__ << "\n";
   }

protected:
   ASTConsumer *CreateASTConsumer(CompilerInstance &CI, StringRef filename)
   {
      //llvm::errs() << __FUNCTION__ << ":" << filename << "\n";
      CreateOutput(CI);
      JSONFormatter* fmt = new JSONFormatter(output_);

      fmt->BeginSourceFile();
      return new ASTVistor(CI, output_, fmt);
   }

#if 0
   virtual bool BeginSourceFileAction(CompilerInstance& CI, StringRef filename)
   {
      llvm::errs() << __FUNCTION__ << ":" << filename << "\n";

      CreateOutput(CI);
      if (output_) {
         *output_ << "{\n";
         // Set to consumer because it could be created before this function invoked.
         if (consumer_) consumer_->SetRawOStream(output_);
         return true;
      }

      return false;
   }

   virtual void EndSourceFileAction()
   {
      llvm::errs() << __FUNCTION__ << "\n";

      *output_ << "}\n";
      output_->flush();
   }
#endif
   bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string>& args)
   {
      for (unsigned i = 0, e = args.size(); i != e; ++i) {
         llvm::errs() << "FunctionFanout arg = " << args[i] << "\n";

         // Example error handling.
         if (args[i] == "-an-error") {
            DiagnosticsEngine &D = CI.getDiagnostics();
            unsigned DiagID = D.getCustomDiagID(DiagnosticsEngine::Error, "invalid argument '" + args[i] + "'");
            D.Report(DiagID);
            return false;
         }
      }
      if (args.size() && args[0] == "help") PrintHelp(llvm::errs());

      return true;
   }
   void PrintHelp(llvm::raw_ostream& ros)
   {
      ros << "Help for FunctionFanout plugin goes here\n";
   }

};

}

static FrontendPluginRegistry::Add<FunctionFanout::ASTAction> X("func-fanout", "collect function fanout");
