#pragma once

#include <Standard_Handle.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <TDataStd_Integer.hxx>
#include <XmlDrivers.hxx>
#include <BinDrivers.hxx>
#include <BinXCAFDrivers.hxx>

#include "scene/CadScene.hpp"

class StepXcafImporter {
public:
    StepXcafImporter();

    bool load(const std::filesystem::path& path, CadDocument& result);

private:
    occ::handle<TDocStd_Application> m_application;
};