/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */

#pragma once

#include "../helpers/common.h"

#ifdef _WIN32

// Published by Visual Studio Setup team
// <copyright file="Setup.Configuration.h" company="Microsoft Corporation">
// Copyright (C) Microsoft Corporation. All rights reserved.
// </copyright>
// This file is licensed under "The MIT License(MIT)".
// This file is released by Visual Studio setup team for consumption by external applications.
// For more information please look at this git repo https://github.com/microsoft/vs-setup-samples
#include <objbase.h>

// Constants
//
#ifndef E_NOTFOUND
#define E_NOTFOUND HRESULT_FROM_WIN32(ERROR_NOT_FOUND)
#endif

#ifndef E_FILENOTFOUND
#define E_FILENOTFOUND HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
#endif

#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)

#ifndef _Outptr_result_maybenull_
#define _Outptr_result_maybenull_
#endif
#ifndef _Out_writes_to_
#define _Out_writes_to_(x, y)
#endif
#ifndef _Reserved_
#define _Reserved_
#endif
#ifndef MAXUINT
#define MAXUINT ((UINT) ~((UINT)0))
#endif

// Enumerations
//
/// <summary>
/// The state of an instance.
/// </summary>
enum InstanceState {
    /// <summary>
    /// The instance state has not been determined.
    /// </summary>
    eNone = 0,

    /// <summary>
    /// The instance installation path exists.
    /// </summary>
    eLocal = 1,

    /// <summary>
    /// A product is registered to the instance.
    /// </summary>
    eRegistered = 2,

    /// <summary>
    /// No reboot is required for the instance.
    /// </summary>
    eNoRebootRequired = 4,

    /// <summary>
    /// The instance represents a complete install.
    /// </summary>
    eComplete = MAXUINT,
};

// Forward interface declarations
//
#ifndef __ISetupInstance_FWD_DEFINED__
#define __ISetupInstance_FWD_DEFINED__
typedef struct ISetupInstance ISetupInstance;
#endif

#ifndef __ISetupInstance2_FWD_DEFINED__
#define __ISetupInstance2_FWD_DEFINED__
typedef struct ISetupInstance2 ISetupInstance2;
#endif

#ifndef __IEnumSetupInstances_FWD_DEFINED__
#define __IEnumSetupInstances_FWD_DEFINED__
typedef struct IEnumSetupInstances IEnumSetupInstances;
#endif

#ifndef __ISetupConfiguration_FWD_DEFINED__
#define __ISetupConfiguration_FWD_DEFINED__
typedef struct ISetupConfiguration ISetupConfiguration;
#endif

#ifndef __ISetupConfiguration2_FWD_DEFINED__
#define __ISetupConfiguration2_FWD_DEFINED__
typedef struct ISetupConfiguration2 ISetupConfiguration2;
#endif

#ifndef __ISetupPackageReference_FWD_DEFINED__
#define __ISetupPackageReference_FWD_DEFINED__
typedef struct ISetupPackageReference ISetupPackageReference;
#endif

#ifndef __ISetupHelper_FWD_DEFINED__
#define __ISetupHelper_FWD_DEFINED__
typedef struct ISetupHelper ISetupHelper;
#endif

// Forward class declarations
//
#ifndef __SetupConfiguration_FWD_DEFINED__
#define __SetupConfiguration_FWD_DEFINED__

#ifdef __cplusplus
typedef class SetupConfiguration SetupConfiguration;
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

// Interface definitions
//
EXTERN_C const IID IID_ISetupInstance;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// Information about an instance of a product.
/// </summary>
struct DECLSPEC_UUID("B41463C3-8866-43B5-BC33-2B0676F7F42E") DECLSPEC_NOVTABLE ISetupInstance : public IUnknown {
    /// <summary>
    /// Gets the instance identifier (should match the name of the parent instance directory).
    /// </summary>
    /// <param name="pbstrInstanceId">The instance identifier.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist.</returns>
    STDMETHOD(GetInstanceId)(_Out_ BSTR *pbstrInstanceId) = 0;

    /// <summary>
    /// Gets the local date and time when the installation was originally installed.
    /// </summary>
    /// <param name="pInstallDate">The local date and time when the installation was originally installed.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetInstallDate)(_Out_ LPFILETIME pInstallDate) = 0;

    /// <summary>
    /// Gets the unique name of the installation, often indicating the branch and other information used for telemetry.
    /// </summary>
    /// <param name="pbstrInstallationName">The unique name of the installation, often indicating the branch and other
    /// information used for telemetry.</param> <returns>Standard HRESULT indicating success or failure, including
    /// E_FILENOTFOUND if the instance state does not exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetInstallationName)(_Out_ BSTR *pbstrInstallationName) = 0;

    /// <summary>
    /// Gets the path to the installation root of the product.
    /// </summary>
    /// <param name="pbstrInstallationPath">The path to the installation root of the product.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetInstallationPath)(_Out_ BSTR *pbstrInstallationPath) = 0;

    /// <summary>
    /// Gets the version of the product installed in this instance.
    /// </summary>
    /// <param name="pbstrInstallationVersion">The version of the product installed in this instance.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetInstallationVersion)(_Out_ BSTR *pbstrInstallationVersion) = 0;

    /// <summary>
    /// Gets the display name (title) of the product installed in this instance.
    /// </summary>
    /// <param name="lcid">The LCID for the display name.</param>
    /// <param name="pbstrDisplayName">The display name (title) of the product installed in this instance.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetDisplayName)(_In_ LCID lcid, _Out_ BSTR *pbstrDisplayName) = 0;

    /// <summary>
    /// Gets the description of the product installed in this instance.
    /// </summary>
    /// <param name="lcid">The LCID for the description.</param>
    /// <param name="pbstrDescription">The description of the product installed in this instance.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetDescription)(_In_ LCID lcid, _Out_ BSTR *pbstrDescription) = 0;

    /// <summary>
    /// Resolves the optional relative path to the root path of the instance.
    /// </summary>
    /// <param name="pwszRelativePath">A relative path within the instance to resolve, or NULL to get the root
    /// path.</param> <param name="pbstrAbsolutePath">The full path to the optional relative path within the instance.
    /// If the relative path is NULL, the root path will always terminate in a backslash.</param> <returns>Standard
    /// HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not exist and
    /// E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(ResolvePath)(_In_opt_z_ LPCOLESTR pwszRelativePath, _Out_ BSTR *pbstrAbsolutePath) = 0;
};
#endif

EXTERN_C const IID IID_ISetupInstance2;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// Information about an instance of a product.
/// </summary>
struct DECLSPEC_UUID("89143C9A-05AF-49B0-B717-72E218A2185C") DECLSPEC_NOVTABLE ISetupInstance2 : public ISetupInstance {
    /// <summary>
    /// Gets the state of the instance.
    /// </summary>
    /// <param name="pState">The state of the instance.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist.</returns>
    STDMETHOD(GetState)(_Out_ InstanceState *pState) = 0;

    /// <summary>
    /// Gets an array of package references registered to the instance.
    /// </summary>
    /// <param name="ppsaPackages">Pointer to an array of <see cref="ISetupPackageReference"/>.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the packages property is not defined.</returns>
    STDMETHOD(GetPackages)(_Out_ LPSAFEARRAY *ppsaPackages) = 0;

    /// <summary>
    /// Gets a pointer to the <see cref="ISetupPackageReference"/> that represents the registered product.
    /// </summary>
    /// <param name="ppPackage">Pointer to an instance of <see cref="ISetupPackageReference"/>. This may be NULL if <see
    /// cref="GetState"/> does not return <see cref="eComplete"/>.</param> <returns>Standard HRESULT indicating success
    /// or failure, including E_FILENOTFOUND if the instance state does not exist and E_NOTFOUND if the packages
    /// property is not defined.</returns>
    STDMETHOD(GetProduct)(_Outptr_result_maybenull_ ISetupPackageReference **ppPackage) = 0;

    /// <summary>
    /// Gets the relative path to the product application, if available.
    /// </summary>
    /// <param name="pbstrProductPath">The relative path to the product application, if available.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist.</returns>
    STDMETHOD(GetProductPath)(_Outptr_result_maybenull_ BSTR *pbstrProductPath) = 0;
};
#endif

EXTERN_C const IID IID_IEnumSetupInstances;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// A enumerator of installed <see cref="ISetupInstance"/> objects.
/// </summary>
struct DECLSPEC_UUID("6380BCFF-41D3-4B2E-8B2E-BF8A6810C848") DECLSPEC_NOVTABLE IEnumSetupInstances : public IUnknown {
    /// <summary>
    /// Retrieves the next set of product instances in the enumeration sequence.
    /// </summary>
    /// <param name="celt">The number of product instances to retrieve.</param>
    /// <param name="rgelt">A pointer to an array of <see cref="ISetupInstance"/>.</param>
    /// <param name="pceltFetched">A pointer to the number of product instances retrieved. If celt is 1 this parameter
    /// may be NULL.</param> <returns>S_OK if the number of elements were fetched, S_FALSE if nothing was fetched (at
    /// end of enumeration), E_INVALIDARG if celt is greater than 1 and pceltFetched is NULL, or E_OUTOFMEMORY if an
    /// <see cref="ISetupInstance"/> could not be allocated.</returns>
    STDMETHOD(Next)
    (_In_ ULONG celt, _Out_writes_to_(celt, *pceltFetched) ISetupInstance **rgelt,
     _Out_opt_ _Deref_out_range_(0, celt) ULONG *pceltFetched) = 0;

    /// <summary>
    /// Skips the next set of product instances in the enumeration sequence.
    /// </summary>
    /// <param name="celt">The number of product instances to skip.</param>
    /// <returns>S_OK if the number of elements could be skipped; otherwise, S_FALSE;</returns>
    STDMETHOD(Skip)(_In_ ULONG celt) = 0;

    /// <summary>
    /// Resets the enumeration sequence to the beginning.
    /// </summary>
    /// <returns>Always returns S_OK;</returns>
    STDMETHOD(Reset)(void) = 0;

    /// <summary>
    /// Creates a new enumeration object in the same state as the current enumeration object: the new object points to
    /// the same place in the enumeration sequence.
    /// </summary>
    /// <param name="ppenum">A pointer to a pointer to a new <see cref="IEnumSetupInstances"/> interface. If the method
    /// fails, this parameter is undefined.</param> <returns>S_OK if a clone was returned; otherwise,
    /// E_OUTOFMEMORY.</returns>
    STDMETHOD(Clone)(_Deref_out_opt_ IEnumSetupInstances **ppenum) = 0;
};
#endif

EXTERN_C const IID IID_ISetupConfiguration;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// Gets information about product instances set up on the machine.
/// </summary>
struct DECLSPEC_UUID("42843719-DB4C-46C2-8E7C-64F1816EFD5B") DECLSPEC_NOVTABLE ISetupConfiguration : public IUnknown {
    /// <summary>
    /// Enumerates all completed product instances installed.
    /// </summary>
    /// <param name="ppEnumInstances">An enumeration of completed, installed product instances.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(EnumInstances)(_Out_ IEnumSetupInstances **ppEnumInstances) = 0;

    /// <summary>
    /// Gets the instance for the current process path.
    /// </summary>
    /// <param name="ppInstance">The instance for the current process path.</param>
    /// <returns>The instance for the current process path, or E_NOTFOUND if not found.</returns>
    STDMETHOD(GetInstanceForCurrentProcess)(_Out_ ISetupInstance **ppInstance) = 0;

    /// <summary>
    /// Gets the instance for the given path.
    /// </summary>
    /// <param name="ppInstance">The instance for the given path.</param>
    /// <returns>The instance for the given path, or E_NOTFOUND if not found.</returns>
    STDMETHOD(GetInstanceForPath)(_In_z_ LPCWSTR wzPath, _Out_ ISetupInstance **ppInstance) = 0;
};
#endif

EXTERN_C const IID IID_ISetupConfiguration2;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// Gets information about product instances.
/// </summary>
struct DECLSPEC_UUID("26AAB78C-4A60-49D6-AF3B-3C35BC93365D") DECLSPEC_NOVTABLE ISetupConfiguration2
    : public ISetupConfiguration {
    /// <summary>
    /// Enumerates all product instances.
    /// </summary>
    /// <param name="ppEnumInstances">An enumeration of all product instances.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(EnumAllInstances)(_Out_ IEnumSetupInstances **ppEnumInstances) = 0;
};
#endif

EXTERN_C const IID IID_ISetupPackageReference;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// A reference to a package.
/// </summary>
struct DECLSPEC_UUID("da8d8a16-b2b6-4487-a2f1-594ccccd6bf5") DECLSPEC_NOVTABLE ISetupPackageReference
    : public IUnknown {
    /// <summary>
    /// Gets the general package identifier.
    /// </summary>
    /// <param name="pbstrId">The general package identifier.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetId)(_Out_ BSTR *pbstrId) = 0;

    /// <summary>
    /// Gets the version of the package.
    /// </summary>
    /// <param name="pbstrVersion">The version of the package.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetVersion)(_Out_ BSTR *pbstrVersion) = 0;

    /// <summary>
    /// Gets the target process architecture of the package.
    /// </summary>
    /// <param name="pbstrChip">The target process architecture of the package.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetChip)(_Out_ BSTR *pbstrChip) = 0;

    /// <summary>
    /// Gets the language and optional region identifier.
    /// </summary>
    /// <param name="pbstrLanguage">The language and optional region identifier.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetLanguage)(_Out_ BSTR *pbstrLanguage) = 0;

    /// <summary>
    /// Gets the build branch of the package.
    /// </summary>
    /// <param name="pbstrBranch">The build branch of the package.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetBranch)(_Out_ BSTR *pbstrBranch) = 0;

    /// <summary>
    /// Gets the type of the package.
    /// </summary>
    /// <param name="pbstrType">The type of the package.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetType)(_Out_ BSTR *pbstrType) = 0;

    /// <summary>
    /// Gets the unique identifier consisting of all defined tokens.
    /// </summary>
    /// <param name="pbstrUniqueId">The unique identifier consisting of all defined tokens.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_UNEXPECTED if no Id was defined
    /// (required).</returns>
    STDMETHOD(GetUniqueId)(_Out_ BSTR *pbstrUniqueId) = 0;
};
#endif

EXTERN_C const IID IID_ISetupHelper;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// Helper functions.
/// </summary>
/// <remarks>
/// You can query for this interface from the <see cref="SetupConfiguration"/> class.
/// </remarks>
struct DECLSPEC_UUID("42b21b78-6192-463e-87bf-d577838f1d5c") DECLSPEC_NOVTABLE ISetupHelper : public IUnknown {
    /// <summary>
    /// Parses a dotted quad version string into a 64-bit unsigned integer.
    /// </summary>
    /// <param name="pwszVersion">The dotted quad version string to parse, e.g. 1.2.3.4.</param>
    /// <param name="pullVersion">A 64-bit unsigned integer representing the version. You can compare this to other
    /// versions.</param> <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(ParseVersion)(_In_ LPCOLESTR pwszVersion, _Out_ PULONGLONG pullVersion) = 0;

    /// <summary>
    /// Parses a dotted quad version string into a 64-bit unsigned integer.
    /// </summary>
    /// <param name="pwszVersionRange">The string containing 1 or 2 dotted quad version strings to parse, e.g. [1.0,)
    /// that means 1.0.0.0 or newer.</param> <param name="pullMinVersion">A 64-bit unsigned integer representing the
    /// minimum version, which may be 0. You can compare this to other versions.</param> <param name="pullMaxVersion">A
    /// 64-bit unsigned integer representing the maximum version, which may be MAXULONGLONG. You can compare this to
    /// other versions.</param> <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(ParseVersionRange)
    (_In_ LPCOLESTR pwszVersionRange, _Out_ PULONGLONG pullMinVersion, _Out_ PULONGLONG pullMaxVersion) = 0;
};
#endif

// Class declarations
//
EXTERN_C const CLSID CLSID_SetupConfiguration;

#ifdef __cplusplus
/// <summary>
/// This class implements <see cref="ISetupConfiguration"/>, <see cref="ISetupConfiguration2"/>, and <see
/// cref="ISetupHelper"/>.
/// </summary>
class DECLSPEC_UUID("177F0C4A-1CD3-4DE7-A32C-71DBBB9FA36D") SetupConfiguration;
#endif

// Function declarations
//
/// <summary>
/// Gets an <see cref="ISetupConfiguration"/> that provides information about product instances installed on the
/// machine.
/// </summary>
/// <param name="ppConfiguration">The <see cref="ISetupConfiguration"/> that provides information about product
/// instances installed on the machine.</param> <param name="pReserved">Reserved for future use.</param>
/// <returns>Standard HRESULT indicating success or failure.</returns>
STDMETHODIMP GetSetupConfiguration(_Out_ ISetupConfiguration **ppConfiguration, _Reserved_ LPVOID pReserved);

#ifdef __cplusplus
}
#endif

#else

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#define VS_SETUP_GCC_DIAGNOSTIC_PUSHED
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#ifndef MAXUINT
#define MAXUINT ((UINT) ~((UINT)0))
#endif

#ifndef DECLSPEC_NOVTABLE
#if (_MSC_VER >= 1100) && defined(__cplusplus)
#define DECLSPEC_NOVTABLE __declspec(novtable)
#else
#define DECLSPEC_NOVTABLE
#endif
#endif

// Enumerations
//
/// <summary>
/// The state of an instance.
/// </summary>
enum InstanceState {
    /// <summary>
    /// The instance state has not been determined.
    /// </summary>
    eNone = 0,

    /// <summary>
    /// The instance installation path exists.
    /// </summary>
    eLocal = 1,

    /// <summary>
    /// A product is registered to the instance.
    /// </summary>
    eRegistered = 2,

    /// <summary>
    /// No reboot is required for the instance.
    /// </summary>
    eNoRebootRequired = 4,

    /// <summary>
    /// The instance represents a complete install.
    /// </summary>
    eComplete = MAXUINT,
};

// Forward interface declarations
//
#ifndef __ISetupInstance_FWD_DEFINED__
#define __ISetupInstance_FWD_DEFINED__
typedef struct ISetupInstance ISetupInstance;
#endif

#ifndef __ISetupInstance2_FWD_DEFINED__
#define __ISetupInstance2_FWD_DEFINED__
typedef struct ISetupInstance2 ISetupInstance2;
#endif

#ifndef __IEnumSetupInstances_FWD_DEFINED__
#define __IEnumSetupInstances_FWD_DEFINED__
typedef struct IEnumSetupInstances IEnumSetupInstances;
#endif

#ifndef __ISetupConfiguration_FWD_DEFINED__
#define __ISetupConfiguration_FWD_DEFINED__
typedef struct ISetupConfiguration ISetupConfiguration;
#endif

#ifndef __ISetupConfiguration2_FWD_DEFINED__
#define __ISetupConfiguration2_FWD_DEFINED__
typedef struct ISetupConfiguration2 ISetupConfiguration2;
#endif

#ifndef __ISetupPackageReference_FWD_DEFINED__
#define __ISetupPackageReference_FWD_DEFINED__
typedef struct ISetupPackageReference ISetupPackageReference;
#endif

#ifndef __ISetupHelper_FWD_DEFINED__
#define __ISetupHelper_FWD_DEFINED__
typedef struct ISetupHelper ISetupHelper;
#endif

// Forward class declarations
//
#ifndef __SetupConfiguration_FWD_DEFINED__
#define __SetupConfiguration_FWD_DEFINED__

#ifdef __cplusplus
typedef class SetupConfiguration SetupConfiguration;
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

// Interface definitions
//
EXTERN_C const IID IID_ISetupInstance;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// Information about an instance of a product.
/// </summary>
struct DECLSPEC_UUID("B41463C3-8866-43B5-BC33-2B0676F7F42E") DECLSPEC_NOVTABLE ISetupInstance : public IUnknown {
    /// <summary>
    /// Gets the instance identifier (should match the name of the parent instance directory).
    /// </summary>
    /// <param name="pbstrInstanceId">The instance identifier.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist.</returns>
    STDMETHOD(GetInstanceId)(BSTR *pbstrInstanceId) = 0;

    /// <summary>
    /// Gets the local date and time when the installation was originally installed.
    /// </summary>
    /// <param name="pInstallDate">The local date and time when the installation was originally installed.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetInstallDate)(LPFILETIME pInstallDate) = 0;

    /// <summary>
    /// Gets the unique name of the installation, often indicating the branch and other information used for telemetry.
    /// </summary>
    /// <param name="pbstrInstallationName">The unique name of the installation, often indicating the branch and other
    /// information used for telemetry.</param> <returns>Standard HRESULT indicating success or failure, including
    /// E_FILENOTFOUND if the instance state does not exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetInstallationName)(BSTR *pbstrInstallationName) = 0;

    /// <summary>
    /// Gets the path to the installation root of the product.
    /// </summary>
    /// <param name="pbstrInstallationPath">The path to the installation root of the product.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetInstallationPath)(BSTR *pbstrInstallationPath) = 0;

    /// <summary>
    /// Gets the version of the product installed in this instance.
    /// </summary>
    /// <param name="pbstrInstallationVersion">The version of the product installed in this instance.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetInstallationVersion)(BSTR *pbstrInstallationVersion) = 0;

    /// <summary>
    /// Gets the display name (title) of the product installed in this instance.
    /// </summary>
    /// <param name="lcid">The LCID for the display name.</param>
    /// <param name="pbstrDisplayName">The display name (title) of the product installed in this instance.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetDisplayName)(LCID lcid, BSTR *pbstrDisplayName) = 0;

    /// <summary>
    /// Gets the description of the product installed in this instance.
    /// </summary>
    /// <param name="lcid">The LCID for the description.</param>
    /// <param name="pbstrDescription">The description of the product installed in this instance.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(GetDescription)(LCID lcid, BSTR *pbstrDescription) = 0;

    /// <summary>
    /// Resolves the optional relative path to the root path of the instance.
    /// </summary>
    /// <param name="pwszRelativePath">A relative path within the instance to resolve, or NULL to get the root
    /// path.</param> <param name="pbstrAbsolutePath">The full path to the optional relative path within the instance.
    /// If the relative path is NULL, the root path will always terminate in a backslash.</param> <returns>Standard
    /// HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not exist and
    /// E_NOTFOUND if the property is not defined.</returns>
    STDMETHOD(ResolvePath)(LPCOLESTR pwszRelativePath, BSTR *pbstrAbsolutePath) = 0;
};
#endif

EXTERN_C const IID IID_ISetupInstance2;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// Information about an instance of a product.
/// </summary>
struct DECLSPEC_UUID("89143C9A-05AF-49B0-B717-72E218A2185C") DECLSPEC_NOVTABLE ISetupInstance2 : public ISetupInstance {
    /// <summary>
    /// Gets the state of the instance.
    /// </summary>
    /// <param name="pState">The state of the instance.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist.</returns>
    STDMETHOD(GetState)(InstanceState *pState) = 0;

    /// <summary>
    /// Gets an array of package references registered to the instance.
    /// </summary>
    /// <param name="ppsaPackages">Pointer to an array of <see cref="ISetupPackageReference"/>.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist and E_NOTFOUND if the packages property is not defined.</returns>
    STDMETHOD(GetPackages)(LPSAFEARRAY *ppsaPackages) = 0;

    /// <summary>
    /// Gets a pointer to the <see cref="ISetupPackageReference"/> that represents the registered product.
    /// </summary>
    /// <param name="ppPackage">Pointer to an instance of <see cref="ISetupPackageReference"/>. This may be NULL if <see
    /// cref="GetState"/> does not return <see cref="eComplete"/>.</param> <returns>Standard HRESULT indicating success
    /// or failure, including E_FILENOTFOUND if the instance state does not exist and E_NOTFOUND if the packages
    /// property is not defined.</returns>
    STDMETHOD(GetProduct)(ISetupPackageReference **ppPackage) = 0;

    /// <summary>
    /// Gets the relative path to the product application, if available.
    /// </summary>
    /// <param name="pbstrProductPath">The relative path to the product application, if available.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_FILENOTFOUND if the instance state does not
    /// exist.</returns>
    STDMETHOD(GetProductPath)(BSTR *pbstrProductPath) = 0;
};
#endif

EXTERN_C const IID IID_IEnumSetupInstances;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// A enumerator of installed <see cref="ISetupInstance"/> objects.
/// </summary>
struct DECLSPEC_UUID("6380BCFF-41D3-4B2E-8B2E-BF8A6810C848") DECLSPEC_NOVTABLE IEnumSetupInstances : public IUnknown {
    /// <summary>
    /// Retrieves the next set of product instances in the enumeration sequence.
    /// </summary>
    /// <param name="celt">The number of product instances to retrieve.</param>
    /// <param name="rgelt">A pointer to an array of <see cref="ISetupInstance"/>.</param>
    /// <param name="pceltFetched">A pointer to the number of product instances retrieved. If celt is 1 this parameter
    /// may be NULL.</param> <returns>S_OK if the number of elements were fetched, S_FALSE if nothing was fetched (at
    /// end of enumeration), E_INVALIDARG if celt is greater than 1 and pceltFetched is NULL, or E_OUTOFMEMORY if an
    /// <see cref="ISetupInstance"/> could not be allocated.</returns>
    STDMETHOD(Next)(ULONG celt, ISetupInstance **rgelt, ULONG *pceltFetched) = 0;

    /// <summary>
    /// Skips the next set of product instances in the enumeration sequence.
    /// </summary>
    /// <param name="celt">The number of product instances to skip.</param>
    /// <returns>S_OK if the number of elements could be skipped; otherwise, S_FALSE;</returns>
    STDMETHOD(Skip)(ULONG celt) = 0;

    /// <summary>
    /// Resets the enumeration sequence to the beginning.
    /// </summary>
    /// <returns>Always returns S_OK;</returns>
    STDMETHOD(Reset)(void) = 0;

    /// <summary>
    /// Creates a new enumeration object in the same state as the current enumeration object: the new object points to
    /// the same place in the enumeration sequence.
    /// </summary>
    /// <param name="ppenum">A pointer to a pointer to a new <see cref="IEnumSetupInstances"/> interface. If the method
    /// fails, this parameter is undefined.</param> <returns>S_OK if a clone was returned; otherwise,
    /// E_OUTOFMEMORY.</returns>
    STDMETHOD(Clone)(IEnumSetupInstances **ppenum) = 0;
};
#endif

EXTERN_C const IID IID_ISetupConfiguration;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// Gets information about product instances set up on the machine.
/// </summary>
struct DECLSPEC_UUID("42843719-DB4C-46C2-8E7C-64F1816EFD5B") DECLSPEC_NOVTABLE ISetupConfiguration : public IUnknown {
    /// <summary>
    /// Enumerates all completed product instances installed.
    /// </summary>
    /// <param name="ppEnumInstances">An enumeration of completed, installed product instances.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(EnumInstances)(IEnumSetupInstances **ppEnumInstances) = 0;

    /// <summary>
    /// Gets the instance for the current process path.
    /// </summary>
    /// <param name="ppInstance">The instance for the current process path.</param>
    /// <returns>The instance for the current process path, or E_NOTFOUND if not found.</returns>
    STDMETHOD(GetInstanceForCurrentProcess)(ISetupInstance **ppInstance) = 0;

    /// <summary>
    /// Gets the instance for the given path.
    /// </summary>
    /// <param name="ppInstance">The instance for the given path.</param>
    /// <returns>The instance for the given path, or E_NOTFOUND if not found.</returns>
    STDMETHOD(GetInstanceForPath)(LPCWSTR wzPath, ISetupInstance **ppInstance) = 0;
};
#endif

EXTERN_C const IID IID_ISetupConfiguration2;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// Gets information about product instances.
/// </summary>
struct DECLSPEC_UUID("26AAB78C-4A60-49D6-AF3B-3C35BC93365D") DECLSPEC_NOVTABLE ISetupConfiguration2
    : public ISetupConfiguration {
    /// <summary>
    /// Enumerates all product instances.
    /// </summary>
    /// <param name="ppEnumInstances">An enumeration of all product instances.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(EnumAllInstances)(IEnumSetupInstances **ppEnumInstances) = 0;
};
#endif

EXTERN_C const IID IID_ISetupPackageReference;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// A reference to a package.
/// </summary>
struct DECLSPEC_UUID("da8d8a16-b2b6-4487-a2f1-594ccccd6bf5") DECLSPEC_NOVTABLE ISetupPackageReference
    : public IUnknown {
    /// <summary>
    /// Gets the general package identifier.
    /// </summary>
    /// <param name="pbstrId">The general package identifier.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetId)(BSTR *pbstrId) = 0;

    /// <summary>
    /// Gets the version of the package.
    /// </summary>
    /// <param name="pbstrVersion">The version of the package.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetVersion)(BSTR *pbstrVersion) = 0;

    /// <summary>
    /// Gets the target process architecture of the package.
    /// </summary>
    /// <param name="pbstrChip">The target process architecture of the package.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetChip)(BSTR *pbstrChip) = 0;

    /// <summary>
    /// Gets the language and optional region identifier.
    /// </summary>
    /// <param name="pbstrLanguage">The language and optional region identifier.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetLanguage)(BSTR *pbstrLanguage) = 0;

    /// <summary>
    /// Gets the build branch of the package.
    /// </summary>
    /// <param name="pbstrBranch">The build branch of the package.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetBranch)(BSTR *pbstrBranch) = 0;

    /// <summary>
    /// Gets the type of the package.
    /// </summary>
    /// <param name="pbstrType">The type of the package.</param>
    /// <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(GetType)(BSTR *pbstrType) = 0;

    /// <summary>
    /// Gets the unique identifier consisting of all defined tokens.
    /// </summary>
    /// <param name="pbstrUniqueId">The unique identifier consisting of all defined tokens.</param>
    /// <returns>Standard HRESULT indicating success or failure, including E_UNEXPECTED if no Id was defined
    /// (required).</returns>
    STDMETHOD(GetUniqueId)(BSTR *pbstrUniqueId) = 0;
};
#endif

EXTERN_C const IID IID_ISetupHelper;

#if defined(__cplusplus) && !defined(CINTERFACE)
/// <summary>
/// Helper functions.
/// </summary>
/// <remarks>
/// You can query for this interface from the <see cref="SetupConfiguration"/> class.
/// </remarks>
struct DECLSPEC_UUID("42b21b78-6192-463e-87bf-d577838f1d5c") DECLSPEC_NOVTABLE ISetupHelper : public IUnknown {
    /// <summary>
    /// Parses a dotted quad version string into a 64-bit unsigned integer.
    /// </summary>
    /// <param name="pwszVersion">The dotted quad version string to parse, e.g. 1.2.3.4.</param>
    /// <param name="pullVersion">A 64-bit unsigned integer representing the version. You can compare this to other
    /// versions.</param> <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(ParseVersion)(LPCOLESTR pwszVersion, PULONGLONG pullVersion) = 0;

    /// <summary>
    /// Parses a dotted quad version string into a 64-bit unsigned integer.
    /// </summary>
    /// <param name="pwszVersionRange">The string containing 1 or 2 dotted quad version strings to parse, e.g. [1.0,)
    /// that means 1.0.0.0 or newer.</param> <param name="pullMinVersion">A 64-bit unsigned integer representing the
    /// minimum version, which may be 0. You can compare this to other versions.</param> <param name="pullMaxVersion">A
    /// 64-bit unsigned integer representing the maximum version, which may be MAXULONGLONG. You can compare this to
    /// other versions.</param> <returns>Standard HRESULT indicating success or failure.</returns>
    STDMETHOD(ParseVersionRange)(LPCOLESTR pwszVersionRange, PULONGLONG pullMinVersion, PULONGLONG pullMaxVersion) = 0;
};
#endif

// Class declarations
//
EXTERN_C const CLSID CLSID_SetupConfiguration;

#ifdef __cplusplus
/// <summary>
/// This class implements <see cref="ISetupConfiguration"/>, <see cref="ISetupConfiguration2"/>, and <see
/// cref="ISetupHelper"/>.
/// </summary>
class DECLSPEC_UUID("177F0C4A-1CD3-4DE7-A32C-71DBBB9FA36D") SetupConfiguration;
#endif

// Function declarations
//
/// <summary>
/// Gets an <see cref="ISetupConfiguration"/> that provides information about product instances installed on the
/// machine.
/// </summary>
/// <param name="ppConfiguration">The <see cref="ISetupConfiguration"/> that provides information about product
/// instances installed on the machine.</param> <param name="pReserved">Reserved for future use.</param>
/// <returns>Standard HRESULT indicating success or failure.</returns>
STDMETHODIMP GetSetupConfiguration(ISetupConfiguration **ppConfiguration, LPVOID pReserved);

#ifdef __cplusplus
}
#endif

#ifdef VS_SETUP_GCC_DIAGNOSTIC_PUSHED
#pragma GCC diagnostic pop
#undef VS_SETUP_GCC_DIAGNOSTIC_PUSHED
#endif

#endif
// Setup.Configuration.h

namespace sw
{

template <class T>
class SmartCOMPtr
{
public:
  SmartCOMPtr() { ptr = NULL; }
  SmartCOMPtr(T* p)
  {
    ptr = p;
    if (ptr != NULL)
      ptr->AddRef();
  }
  SmartCOMPtr(const SmartCOMPtr<T>& sptr)
  {
    ptr = sptr.ptr;
    if (ptr != NULL)
      ptr->AddRef();
  }
  T** operator&() { return &ptr; }
  T* operator->() { return ptr; }
  T* operator=(T* p)
  {
    if (*this != p) {
      ptr = p;
      if (ptr != NULL)
        ptr->AddRef();
    }
    return *this;
  }
  operator T*() const { return ptr; }
  template <class I>
  HRESULT QueryInterface(REFCLSID rclsid, I** pp)
  {
    if (pp != NULL) {
      return ptr->QueryInterface(rclsid, (void**)pp);
    } else {
      return E_FAIL;
    }
  }
  HRESULT CoCreateInstance(REFCLSID clsid, IUnknown* pUnknown,
                           REFIID interfaceId, DWORD dwClsContext = CLSCTX_ALL)
  {
    HRESULT hr = ::CoCreateInstance(clsid, pUnknown, dwClsContext, interfaceId,
                                    (void**)&ptr);
    return hr;
  }
  ~SmartCOMPtr()
  {
    if (ptr != NULL)
      ptr->Release();
  }

private:
  T* ptr;
};

class SmartBSTR
{
public:
  SmartBSTR() { str = NULL; }
  SmartBSTR(const SmartBSTR& src)
  {
    if (src.str != NULL) {
      str = ::SysAllocStringByteLen((char*)str, ::SysStringByteLen(str));
    } else {
      str = ::SysAllocStringByteLen(NULL, 0);
    }
  }
  SmartBSTR& operator=(const SmartBSTR& src)
  {
    if (str != src.str) {
      ::SysFreeString(str);
      if (src.str != NULL) {
        str = ::SysAllocStringByteLen((char*)str, ::SysStringByteLen(str));
      } else {
        str = ::SysAllocStringByteLen(NULL, 0);
      }
    }
    return *this;
  }
  operator BSTR() const { return str; }
  BSTR* operator&() throw() { return &str; }
  ~SmartBSTR() throw() { ::SysFreeString(str); }
private:
  BSTR str;
};

struct VSInstanceInfo {
    //std::wstring InstanceId;
    std::wstring VSInstallLocation;
    std::wstring Version;
    //uint64_t ullVersion{0};
    //bool IsWin10SDKInstalled{false};
    //bool IsWin81SDKInstalled{false};
};

struct cmVSSetupAPIHelper
{
  std::vector<VSInstanceInfo> instances;

  cmVSSetupAPIHelper();
  ~cmVSSetupAPIHelper();

  bool EnumerateVSInstances();

private:
  bool Initialize();
  bool GetVSInstanceInfo(SmartCOMPtr<ISetupInstance2> instance2,
                         VSInstanceInfo& vsInstanceInfo);
  bool CheckInstalledComponent(SmartCOMPtr<ISetupPackageReference> package,
                               bool& bVCToolset, bool& bWin10SDK,
                               bool& bWin81SDK);

  // COM ptrs to query about VS instances
  SmartCOMPtr<ISetupConfiguration> setupConfig;
  SmartCOMPtr<ISetupConfiguration2> setupConfig2;
  SmartCOMPtr<ISetupHelper> setupHelper;
  // used to indicate failure in Initialize(), so we don't have to call again
  bool initializationFailure;
};

#ifndef VSSetupConstants
#define VSSetupConstants
/* clang-format off */
const IID IID_ISetupConfiguration = {
  0x42843719, 0xDB4C, 0x46C2,
  { 0x8E, 0x7C, 0x64, 0xF1, 0x81, 0x6E, 0xFD, 0x5B }
};
const IID IID_ISetupConfiguration2 = {
  0x26AAB78C, 0x4A60, 0x49D6,
  { 0xAF, 0x3B, 0x3C, 0x35, 0xBC, 0x93, 0x36, 0x5D }
};
const IID IID_ISetupPackageReference = {
  0xda8d8a16, 0xb2b6, 0x4487,
  { 0xa2, 0xf1, 0x59, 0x4c, 0xcc, 0xcd, 0x6b, 0xf5 }
};
const IID IID_ISetupHelper = {
  0x42b21b78, 0x6192, 0x463e,
  { 0x87, 0xbf, 0xd5, 0x77, 0x83, 0x8f, 0x1d, 0x5c }
};
const IID IID_IEnumSetupInstances = {
  0x6380BCFF, 0x41D3, 0x4B2E,
  { 0x8B, 0x2E, 0xBF, 0x8A, 0x68, 0x10, 0xC8, 0x48 }
};
const IID IID_ISetupInstance2 = {
  0x89143C9A, 0x05AF, 0x49B0,
  { 0xB7, 0x17, 0x72, 0xE2, 0x18, 0xA2, 0x18, 0x5C }
};
const IID IID_ISetupInstance = {
  0xB41463C3, 0x8866, 0x43B5,
  { 0xBC, 0x33, 0x2B, 0x06, 0x76, 0xF7, 0xF4, 0x2E }
};
const CLSID CLSID_SetupConfiguration = {
  0x177F0C4A, 0x1CD3, 0x4DE7,
  { 0xA3, 0x2C, 0x71, 0xDB, 0xBB, 0x9F, 0xA3, 0x6D }
};
/* clang-format on */
#endif

const WCHAR *VCToolsetComponent = L"Microsoft.VisualStudio.Component.VC.Tools.x86.x64";
const WCHAR *Win10SDKComponent = L"Microsoft.VisualStudio.Component.Windows10SDK";
const WCHAR *Win81SDKComponent = L"Microsoft.VisualStudio.Component.Windows81SDK";
const WCHAR *ComponentType = L"Component";

cmVSSetupAPIHelper::cmVSSetupAPIHelper()
    : setupConfig(NULL), setupConfig2(NULL), setupHelper(NULL), initializationFailure(false) {
    Initialize();
}

cmVSSetupAPIHelper::~cmVSSetupAPIHelper() {
    setupHelper = NULL;
    setupConfig2 = NULL;
    setupConfig = NULL;
}

bool cmVSSetupAPIHelper::CheckInstalledComponent(SmartCOMPtr<ISetupPackageReference> package, bool &bVCToolset,
                                                 bool &bWin10SDK, bool &bWin81SDK) {
    bool ret = false;
    bVCToolset = bWin10SDK = bWin81SDK = false;
    SmartBSTR bstrId;
    if (FAILED(package->GetId(&bstrId))) {
        return ret;
    }

    SmartBSTR bstrType;
    if (FAILED(package->GetType(&bstrType))) {
        return ret;
    }

    std::wstring id = std::wstring(bstrId);
    std::wstring type = std::wstring(bstrType);
    if (id.compare(VCToolsetComponent) == 0 && type.compare(ComponentType) == 0) {
        bVCToolset = true;
        ret = true;
    }

    // Checks for any version of Win10 SDK. The version is appended at the end of
    // the
    // component name ex: Microsoft.VisualStudio.Component.Windows10SDK.10240
    if (id.find(Win10SDKComponent) != std::wstring::npos && type.compare(ComponentType) == 0) {
        bWin10SDK = true;
        ret = true;
    }

    if (id.compare(Win81SDKComponent) == 0 && type.compare(ComponentType) == 0) {
        bWin81SDK = true;
        ret = true;
    }

    return ret;
}

// Gather additional info such as if VCToolset, WinSDKs are installed, location
// of VS and version information.
bool cmVSSetupAPIHelper::GetVSInstanceInfo(SmartCOMPtr<ISetupInstance2> pInstance, VSInstanceInfo &vsInstanceInfo) {
    bool isVCToolSetInstalled = false;
    if (pInstance == NULL)
        return false;

    SmartBSTR bstrId;
    if (SUCCEEDED(pInstance->GetInstanceId(&bstrId))) {
        //vsInstanceInfo.InstanceId = std::wstring(bstrId);
    } else {
        return false;
    }

    InstanceState state;
    if (FAILED(pInstance->GetState(&state))) {
        return false;
    }

    ULONGLONG ullVersion = 0;
    SmartBSTR bstrVersion;
    if (FAILED(pInstance->GetInstallationVersion(&bstrVersion))) {
        return false;
    } else {
        vsInstanceInfo.Version = std::wstring(bstrVersion);
        if (FAILED(setupHelper->ParseVersion(bstrVersion, &ullVersion))) {
            //vsInstanceInfo.ullVersion = 0;
        } else {
            //vsInstanceInfo.ullVersion = ullVersion;
        }
    }

    // Reboot may have been required before the installation path was created.
    SmartBSTR bstrInstallationPath;
    if ((eLocal & state) == eLocal) {
        if (FAILED(pInstance->GetInstallationPath(&bstrInstallationPath))) {
            return false;
        } else {
            vsInstanceInfo.VSInstallLocation = std::wstring(bstrInstallationPath);
        }
    }

    // Reboot may have been required before the product package was registered
    // (last).
    if ((eRegistered & state) == eRegistered) {
        SmartCOMPtr<ISetupPackageReference> product;
        if (FAILED(pInstance->GetProduct(&product)) || !product) {
            return false;
        }

        LPSAFEARRAY lpsaPackages;
        if (FAILED(pInstance->GetPackages(&lpsaPackages)) || lpsaPackages == NULL) {
            return false;
        }

        int lower = lpsaPackages->rgsabound[0].lLbound;
        int upper = lpsaPackages->rgsabound[0].cElements + lower;

        IUnknown **ppData = (IUnknown **)lpsaPackages->pvData;
        for (int i = lower; i < upper; i++) {
            SmartCOMPtr<ISetupPackageReference> package = NULL;
            if (FAILED(ppData[i]->QueryInterface(IID_ISetupPackageReference, (void **)&package)) || package == NULL)
                continue;

            bool vcToolsetInstalled = false, win10SDKInstalled = false, win81SDkInstalled = false;
            bool ret = CheckInstalledComponent(package, vcToolsetInstalled, win10SDKInstalled, win81SDkInstalled);
            if (ret) {
                isVCToolSetInstalled |= vcToolsetInstalled;
                //vsInstanceInfo.IsWin10SDKInstalled |= win10SDKInstalled;
                //vsInstanceInfo.IsWin81SDKInstalled |= win81SDkInstalled;
            }
        }

        SafeArrayDestroy(lpsaPackages);
    }

    return isVCToolSetInstalled;
}

bool cmVSSetupAPIHelper::EnumerateVSInstances() {
    bool isVSInstanceExists = false;

    if (initializationFailure || setupConfig == NULL || setupConfig2 == NULL || setupHelper == NULL)
        return false;

    SmartCOMPtr<IEnumSetupInstances> enumInstances = NULL;
    if (FAILED(setupConfig2->EnumInstances((IEnumSetupInstances **)&enumInstances)) || !enumInstances) {
        return false;
    }

    SmartCOMPtr<ISetupInstance> instance;
    while (SUCCEEDED(enumInstances->Next(1, &instance, NULL)) && instance) {
        SmartCOMPtr<ISetupInstance2> instance2 = NULL;
        if (FAILED(instance->QueryInterface(IID_ISetupInstance2, (void **)&instance2)) || !instance2) {
            instance = NULL;
            continue;
        }

        VSInstanceInfo instanceInfo;
        bool isInstalled = GetVSInstanceInfo(instance2, instanceInfo);
        instance = instance2 = NULL;

        if (isInstalled) {
            instances.push_back(instanceInfo);
        }
    }

    if (instances.size() > 0) {
        isVSInstanceExists = true;
    }

    return isVSInstanceExists;
}

bool cmVSSetupAPIHelper::Initialize() {
    if (FAILED(setupConfig.CoCreateInstance(CLSID_SetupConfiguration, NULL, IID_ISetupConfiguration,
                                            CLSCTX_INPROC_SERVER)) ||
        setupConfig == NULL) {
        initializationFailure = true;
        return false;
    }

    if (FAILED(setupConfig.QueryInterface(IID_ISetupConfiguration2, (void **)&setupConfig2)) || setupConfig2 == NULL) {
        initializationFailure = true;
        return false;
    }

    if (FAILED(setupConfig.QueryInterface(IID_ISetupHelper, (void **)&setupHelper)) || setupHelper == NULL) {
        initializationFailure = true;
        return false;
    }

    initializationFailure = false;
    return true;
}

inline std::vector<VSInstanceInfo> enumerate_vs_instances() {
    auto r = CoInitializeEx(0, 0);
    if (r != S_OK) {
    }
    scope_exit se{[]{ CoUninitialize(); }};

    cmVSSetupAPIHelper h;
    if (!h.EnumerateVSInstances()) {
        throw std::runtime_error("can't enumerate vs instances");
    }
    return h.instances;
}


} // namespace sw

#endif // #ifdef _WIN32
