#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

#include "crypto.hpp"
#include "export.hpp"
#include "vault.hpp"

namespace
{
    bool rawVaultsEqual(
        const vault::RawVault &left,
        const vault::RawVault &right)
    {
        return left.salt == right.salt
            && left.nonce == right.nonce
            && left.cipherText == right.cipherText
            && left.formatVersion == right.formatVersion;
    }

    class TemporaryVaultPath
    {
    public:
        TemporaryVaultPath()
            : path_(
                  std::filesystem::temp_directory_path()
                  / ("passman-test-" + std::to_string(
                         static_cast<unsigned long long>(
                             std::filesystem::file_time_type::clock::now()
                                 .time_since_epoch()
                                 .count()))))
        {
        }

        ~TemporaryVaultPath()
        {
            std::error_code error;
            std::filesystem::remove(path_, error);
            std::filesystem::remove(path_.string() + ".tmp", error);
            std::filesystem::remove(path_.string() + ".export", error);
            std::filesystem::remove(path_.string() + ".imported", error);
            std::filesystem::remove(path_.string() + ".imported.tmp", error);
            std::filesystem::remove(path_.string() + ".malformed", error);
        }

        const std::filesystem::path &path() const
        {
            return path_;
        }

    private:
        std::filesystem::path path_;
    };

    void assertEntries(const vault::Vault &storedVault)
    {
        const auto &entries = storedVault.list();
        if (entries.size() != 2) throw std::runtime_error("Expected 2 entries");
        if (entries[0].site != "example.com") throw std::runtime_error("Wrong site for entry 0");
        if (entries[0].username != "alice") throw std::runtime_error("Wrong username for entry 0");
        if (entries[0].password != "first-secret") throw std::runtime_error("Wrong password for entry 0");
        if (entries[1].site != "mail.example.com") throw std::runtime_error("Wrong site for entry 1");
        if (entries[1].notes != "personal account") throw std::runtime_error("Wrong notes for entry 1");
    }
}

int main()
{
    crypto::initialize();
    TemporaryVaultPath temporaryVault;
    const std::string path = temporaryVault.path().string();
    const std::string oldPassword = "old-password";
    const std::string newPassword = "new-password";
    const std::string thirdPassword = "third-password";

    vault::Vault storedVault = vault::Vault::create(path, oldPassword);
    assert(vault::vaultExists(path));
    struct stat vaultStatus;
    assert(stat(path.c_str(), &vaultStatus) == 0);
    assert((vaultStatus.st_mode & (S_IRWXG | S_IRWXO)) == 0);
    assert(vault::loadRaw(path).salt.size() == vault::SALT_BYTES);
    assert(vault::loadRaw(path).nonce.size() == vault::NONCE_BYTES);
    storedVault.add({"example.com", "alice", "first-secret", "work account"});
    storedVault.add(
        {"mail.example.com", "alice-mail", "second-secret", "personal account"});
    storedVault.save();
    assertEntries(storedVault);
    assert(!std::filesystem::exists(path + ".tmp"));
    const vault::RawVault beforeRotation = vault::loadRaw(path);

    bool duplicateRejected = false;
    try
    {
        storedVault.add({"example.com", "other", "other-secret", ""});
    }
    catch (const std::runtime_error &)
    {
        duplicateRejected = true;
    }
    if (!duplicateRejected) throw std::runtime_error("Expected duplicate to be rejected");
    assert(storedVault.get("example.com").password == "first-secret");

    storedVault.remove("mail.example.com");
    assert(storedVault.list().size() == 1);
    storedVault.add(
        {"mail.example.com", "alice-mail", "second-secret", "personal account"});
    assertEntries(storedVault);
    storedVault.save();
    const vault::RawVault beforePasswordRotation = vault::loadRaw(path);

    bool incorrectCurrentRejected = false;
    try
    {
        storedVault.changeMasterPassword("incorrect-password", newPassword);
    }
    catch (const std::runtime_error &)
    {
        incorrectCurrentRejected = true;
    }
    if (!incorrectCurrentRejected) throw std::runtime_error("Expected incorrect password to be rejected");
    assert(rawVaultsEqual(
        beforePasswordRotation,
        vault::loadRaw(path)));

    bool emptyNewPasswordRejected = false;
    try
    {
        storedVault.changeMasterPassword(oldPassword, "");
    }
    catch (const std::runtime_error &)
    {
        emptyNewPasswordRejected = true;
    }
    if (!emptyNewPasswordRejected) throw std::runtime_error("Expected empty password to be rejected");
    assert(rawVaultsEqual(
        beforePasswordRotation,
        vault::loadRaw(path)));

    storedVault.changeMasterPassword(oldPassword, newPassword);
    assertEntries(storedVault);
    const vault::RawVault afterRotation = vault::loadRaw(path);
    assert(afterRotation.salt != beforePasswordRotation.salt);
    assert(afterRotation.nonce != beforePasswordRotation.nonce);
    assert(afterRotation.cipherText != beforePasswordRotation.cipherText);

    bool oldPasswordRejected = false;
    try
    {
        static_cast<void>(vault::Vault::open(path, oldPassword));
    }
    catch (const std::runtime_error &)
    {
        oldPasswordRejected = true;
    }
    if (!oldPasswordRejected) throw std::runtime_error("Expected old password to be rejected");

    vault::Vault reopenedVault = vault::Vault::open(path, newPassword);
    assertEntries(reopenedVault);

    bool oldPasswordRejectedAgain = false;
    try
    {
        static_cast<void>(vault::Vault::open(path, oldPassword));
    }
    catch (const std::runtime_error &)
    {
        oldPasswordRejectedAgain = true;
    }
    if (!oldPasswordRejectedAgain) throw std::runtime_error("Expected old password to be rejected again");

    reopenedVault.changeMasterPassword(newPassword, thirdPassword);
    const vault::RawVault afterSecondRotation = vault::loadRaw(path);
    assert(afterSecondRotation.salt != afterRotation.salt);
    assert(afterSecondRotation.nonce != afterRotation.nonce);
    assert(afterSecondRotation.cipherText != afterRotation.cipherText);
    vault::Vault thirdVault = vault::Vault::open(path, thirdPassword);
    assertEntries(thirdVault);

    const std::filesystem::path exportPath =
        temporaryVault.path().string() + ".export";
    export_format::write(exportPath.string(), thirdVault.list());
    assert(export_format::read(exportPath.string()) == thirdVault.list());

    bool existingExportRejected = false;
    try
    {
        export_format::write(exportPath.string(), thirdVault.list());
    }
    catch (const std::runtime_error &)
    {
        existingExportRejected = true;
    }
    if (!existingExportRejected) throw std::runtime_error("Expected existing export to be rejected");

    vault::Vault importedVault = vault::Vault::create(
        temporaryVault.path().string() + ".imported", "import-password");
    importedVault.importEntries(thirdVault.list());
    assertEntries(importedVault);

    bool duplicateImportRejected = false;
    try
    {
        importedVault.importEntries(thirdVault.list());
    }
    catch (const std::runtime_error &)
    {
        duplicateImportRejected = true;
    }
    if (!duplicateImportRejected) throw std::runtime_error("Expected duplicate import to be rejected");
    assertEntries(importedVault);

    const std::filesystem::path malformedExport =
        temporaryVault.path().string() + ".malformed";
    {
        std::ofstream file(malformedExport, std::ios::binary);
        file << "not-an-export";
    }
    bool malformedExportRejected = false;
    try
    {
        static_cast<void>(export_format::read(malformedExport.string()));
    }
    catch (const std::runtime_error &)
    {
        malformedExportRejected = true;
    }
    if (!malformedExportRejected) throw std::runtime_error("Expected malformed export to be rejected");

    bool secondPasswordRejected = false;
    try
    {
        static_cast<void>(vault::Vault::open(path, newPassword));
    }
    catch (const std::runtime_error &)
    {
        secondPasswordRejected = true;
    }
    if (!secondPasswordRejected) throw std::runtime_error("Expected second password to be rejected");

    const vault::RawVault raw = vault::loadRaw(path);
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        assert(file);
        file.put('P');
        file.put('M');
        file.put('V');
        file.put('1');
        file.put(static_cast<char>(vault::CURRENT_FORMAT_VERSION + 1));
        file.write(
            reinterpret_cast<const char *>(raw.salt.data()),
            static_cast<std::streamsize>(raw.salt.size()));
        file.write(
            reinterpret_cast<const char *>(raw.nonce.data()),
            static_cast<std::streamsize>(raw.nonce.size()));
        file.write(
            reinterpret_cast<const char *>(raw.cipherText.data()),
            static_cast<std::streamsize>(raw.cipherText.size()));
    }

    bool unsupportedVersionRejected = false;
    try
    {
        static_cast<void>(vault::Vault::open(path, newPassword));
    }
    catch (const std::runtime_error &error)
    {
        unsupportedVersionRejected =
            std::string(error.what()).find("Unsupported vault format version")
            != std::string::npos;
    }
    if (!unsupportedVersionRejected) throw std::runtime_error("Expected unsupported version to be rejected");

    {
        std::ofstream invalidHeader(path, std::ios::binary | std::ios::trunc);
        assert(invalidHeader);
        invalidHeader.put('X');
        invalidHeader.put('M');
        invalidHeader.put('V');
        invalidHeader.put('1');
        invalidHeader.put(static_cast<char>(vault::CURRENT_FORMAT_VERSION));
        invalidHeader.write(
            reinterpret_cast<const char *>(raw.salt.data()),
            static_cast<std::streamsize>(raw.salt.size()));
        invalidHeader.write(
            reinterpret_cast<const char *>(raw.nonce.data()),
            static_cast<std::streamsize>(raw.nonce.size()));
        invalidHeader.write(
            reinterpret_cast<const char *>(raw.cipherText.data()),
            static_cast<std::streamsize>(raw.cipherText.size()));
    }

    bool invalidHeaderRejected = false;
    try
    {
        static_cast<void>(vault::Vault::open(path, thirdPassword));
    }
    catch (const std::runtime_error &)
    {
        invalidHeaderRejected = true;
    }
    if (!invalidHeaderRejected) throw std::runtime_error("Expected invalid header to be rejected");

    std::ofstream truncatedFile(path, std::ios::binary | std::ios::trunc);
    assert(truncatedFile);
    truncatedFile.put('P');
    truncatedFile.put('M');
    truncatedFile.put('V');
    truncatedFile.put('1');
    truncatedFile.put(static_cast<char>(vault::CURRENT_FORMAT_VERSION));
    truncatedFile.close();

    bool corruptedVaultRejected = false;
    try
    {
        static_cast<void>(vault::Vault::open(path, newPassword));
    }
    catch (const std::runtime_error &)
    {
        corruptedVaultRejected = true;
    }
    if (!corruptedVaultRejected) throw std::runtime_error("Expected corrupted vault to be rejected");

    vault::saveRaw(path, afterSecondRotation);
    vault::RawVault validRaw = vault::loadRaw(path);
    validRaw.cipherText.back() ^= 0x01U;
    vault::saveRaw(path, validRaw);

    bool tamperedVaultRejected = false;
    try
    {
        static_cast<void>(vault::Vault::open(path, newPassword));
    }
    catch (const std::runtime_error &)
    {
        tamperedVaultRejected = true;
    }
    if (!tamperedVaultRejected) throw std::runtime_error("Expected tampered vault to be rejected");

    vault::saveRaw(path, afterSecondRotation);
    bool invalidRawRejected = false;
    try
    {
        vault::RawVault invalidRaw = afterSecondRotation;
        invalidRaw.salt.clear();
        vault::saveRaw(path, invalidRaw);
    }
    catch (const std::runtime_error &)
    {
        invalidRawRejected = true;
    }
    if (!invalidRawRejected) throw std::runtime_error("Expected invalid raw vault to be rejected");
    assert(rawVaultsEqual(afterSecondRotation, vault::loadRaw(path)));
    assert(!rawVaultsEqual(beforeRotation, afterRotation));

    std::cout << "Vault tests passed.\n";
    return 0;
}
