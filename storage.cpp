#include "ole/olecomm.h"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
template<class Interface> struct Release {
    void operator()(Interface* object) const { if (object) object->Release(); }
};
template<class Interface> using Pointer = std::unique_ptr<Interface, Release<Interface>>;

void checkStorage(HRESULT result, const char* operation) {
    if (FAILED(result)) throw std::runtime_error(std::string(operation) + ": OLE error " + std::to_string(result));
}

std::vector<OLECHAR> oleString(const std::string& text) {
    std::vector<OLECHAR> result(text.begin(), text.end());
    result.push_back(0);
    return result;
}
}

#ifdef MIX_TESTING
void makeTestDocument(const std::string& input, const std::string& output) {
    IStorage* rawSource = nullptr;
    checkStorage(StgOpenStorage(input.c_str(), nullptr, OLE_READ_ONLY_MODE,
        nullptr, 0, &rawSource), "Open fixture image");
    Pointer<IStorage> source(rawSource);
    IStorage* rawRoot = nullptr;
    checkStorage(StgCreateDocfile(output.c_str(), OLE_CREATE_MODE, 0, &rawRoot), "Create MIX fixture");
    Pointer<IStorage> root(rawRoot);
    STATSTG information{};
    checkStorage(source->Stat(&information, STATFLAG_NONAME), "Read fixture class");
    for (const auto* name : {"Data Object Store 000001", "Data Object Store 000044"}) {
        IStorage* rawImage = nullptr;
        checkStorage(root->CreateStorage(name, OLE_CREATE_MODE, 0, 0, &rawImage), "Create fixture storage");
        Pointer<IStorage> image(rawImage);
        checkStorage(image->SetClass(information.clsid), "Set fixture class");
        checkStorage(source->CopyTo(0, nullptr, nullptr, image.get()), "Copy fixture pixels");
        checkStorage(image->Commit(STGC_DEFAULT), "Commit fixture image");
    }
    checkStorage(root->Commit(STGC_DEFAULT), "Commit MIX fixture");
}
#endif

void extractStorage(const std::string& input, const std::string& name, const std::string& output) {
    auto inputName = oleString(input);
    auto storageName = oleString(name);
    auto outputName = oleString(output);
    IStorage* rawRoot = nullptr;
    checkStorage(StgOpenStorage(reinterpret_cast<char*>(inputName.data()), nullptr,
        OLE_READ_ONLY_MODE, nullptr, 0, &rawRoot), "Open MIX storage");
    Pointer<IStorage> root(rawRoot);
    IStorage* rawSource = nullptr;
    checkStorage(root->OpenStorage(reinterpret_cast<char*>(storageName.data()), nullptr,
        OLE_READ_ONLY_MODE, nullptr, 0, &rawSource), "Open embedded storage");
    Pointer<IStorage> source(rawSource);
    IStorage* rawDestination = nullptr;
    checkStorage(StgCreateDocfile(reinterpret_cast<char*>(outputName.data()), OLE_CREATE_MODE,
        0, &rawDestination), "Create standalone FlashPix");
    Pointer<IStorage> destination(rawDestination);
    STATSTG information{};
    checkStorage(source->Stat(&information, STATFLAG_NONAME), "Read storage class");
    checkStorage(destination->SetClass(information.clsid), "Set FlashPix class");
    checkStorage(source->CopyTo(0, nullptr, nullptr, destination.get()), "Copy embedded image");
    checkStorage(destination->Commit(STGC_DEFAULT), "Commit standalone FlashPix");
}

std::vector<std::string> imageStorages(const std::string& input) {
    auto inputName = oleString(input);
    IStorage* rawRoot = nullptr;
    checkStorage(StgOpenStorage(reinterpret_cast<char*>(inputName.data()), nullptr,
        OLE_READ_ONLY_MODE, nullptr, 0, &rawRoot), "Open document directory");
    Pointer<IStorage> root(rawRoot);
    IEnumSTATSTG* rawEntries = nullptr;
    checkStorage(root->EnumElements(0, nullptr, 0, &rawEntries), "List document storages");
    Pointer<IEnumSTATSTG> entries(rawEntries);
    std::vector<std::string> result;
    for (;;) {
        STATSTG information{};
        ULONG fetched = 0;
        const auto code = entries->Next(1, &information, &fetched);
        checkStorage(code, "Read directory entry");
        if (!fetched) break;
        std::string name(information.pwcsName);
        CoTaskMemFree(information.pwcsName);
        if (information.type == STGTY_STORAGE && name.rfind("Data Object Store ", 0) == 0 &&
            name.size() == 24 && std::all_of(name.begin() + 18, name.end(),
                [](char digit) { return digit >= '0' && digit <= '9'; })) {
            result.push_back(name);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}