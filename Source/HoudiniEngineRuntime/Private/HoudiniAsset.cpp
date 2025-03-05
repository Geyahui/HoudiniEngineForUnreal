/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "T2HoudiniAsset.h"
#include "HoudiniPluginSerializationVersion.h"

#include "Misc/Paths.h"
#include "HAL/UnrealMemory.h"

UT2HoudiniAsset::UT2HoudiniAsset(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
	, AssetFileName(TEXT(""))
	, AssetBytesCount(0)	
	, bAssetLimitedCommercial(false)
	, bAssetNonCommercial(false)
	, bAssetExpanded(false)
{}

void
UT2HoudiniAsset::CreateAsset(const uint8 * BufferStart, const uint8 * BufferEnd, const FString & InFileName)
{
	AssetFileName = InFileName;

	// Calculate buffer size.
	AssetBytesCount = BufferEnd - BufferStart;

	if (AssetBytesCount)
	{
		// Allocate buffer to store the raw data.
		AssetBytes.SetNumUninitialized(AssetBytesCount);
		// Copy data into the newly allocated buffer.
		FMemory::Memcpy(AssetBytes.GetData(), BufferStart, AssetBytesCount);
	}

	FString FileExtension = FPaths::GetExtension(InFileName);

	// Expanded HDAs are imported via a "houdini.hdalibrary" file inside the .hda directory
	// Identify them first, then update the file path to point to the .hda dir
	if (FileExtension.Equals(TEXT("hdalibrary"), ESearchCase::IgnoreCase))
	{
		bAssetExpanded = true;

		// Use the parent ".hda" directory as the filename
		AssetFileName = FPaths::GetPath(AssetFileName);
		FileExtension = FPaths::GetExtension(AssetFileName);
	}
	
	if (FileExtension.Equals(TEXT("hdalc"), ESearchCase::IgnoreCase)
		|| FileExtension.Equals(TEXT("otlc"), ESearchCase::IgnoreCase))
	{
		// Check if the HDA is limited (Indie) ...
		bAssetLimitedCommercial = true;
	}
	else if (FileExtension.Equals(TEXT("hdanc"), ESearchCase::IgnoreCase)
		|| FileExtension.Equals(TEXT("otlnc"), ESearchCase::IgnoreCase))
	{
		// ... or non commercial (Apprentice)
		bAssetNonCommercial = true;
	}
}

void
UT2HoudiniAsset::FinishDestroy()
{
	// Release buffer which was used to store raw OTL data.
	AssetBytes.Empty();
	Super::FinishDestroy();
}

const uint8 *
UT2HoudiniAsset::GetAssetBytes() const
{
	return AssetBytes.GetData();
}

const FString &
UT2HoudiniAsset::GetAssetFileName() const
{
	return AssetFileName;
}

uint32
UT2HoudiniAsset::GetAssetBytesCount() const
{
	return AssetBytesCount;
}

void
UT2HoudiniAsset::Serialize(FArchive & Ar)
{
	// Serializes our UProperties
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FHoudiniCustomSerializationVersion::GUID);

	// Get the version
	uint32 HoudiniAssetVersion = Ar.CustomVer(FHoudiniCustomSerializationVersion::GUID);

	// Only version 1 assets needs manual serialization
	if ( HoudiniAssetVersion < VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_V2_BASE 
		|| HoudiniAssetVersion > VER_HOUDINI_PLUGIN_SERIALIZATION_AUTOMATIC_VERSION )
		return SerializeLegacy(Ar);
}

void
UT2HoudiniAsset::SerializeLegacy(FArchive & Ar)
{
	uint32 FileFormatVersion;
	Ar << FileFormatVersion;

	Ar << AssetBytesCount;
	if (Ar.IsLoading())
	{
		// Allocate sufficient space to read stored raw OTL data.
		AssetBytes.SetNumUninitialized(AssetBytesCount);
	}

	if (AssetBytesCount)
		Ar.Serialize(AssetBytes.GetData(), AssetBytesCount);

	// Serialized flags.
	union
	{
		struct
		{
			uint32 bLegacyPreviewHoudiniLogo : 1;
			uint32 bLegacyAssetLimitedCommercial : 1;
			uint32 bLegacyAssetNonCommercial : 1;
		};
		uint32 HoudiniAssetFlagsPacked;
	};
	Ar << HoudiniAssetFlagsPacked;

	bAssetNonCommercial = bLegacyAssetNonCommercial;
	bAssetLimitedCommercial = bLegacyAssetLimitedCommercial;

	// Serialize asset file path.
	Ar << AssetFileName;
}

void
UT2HoudiniAsset::GetAssetRegistryTags(TArray< FAssetRegistryTag > & OutTags) const
{
	// Filename
	OutTags.Add(FAssetRegistryTag("FileName", AssetFileName, FAssetRegistryTag::TT_Alphabetical));

	// Bytes
	OutTags.Add(FAssetRegistryTag("Bytes", FString::FromInt(AssetBytesCount), FAssetRegistryTag::TT_Numerical));

	// Indicate if the Asset is Full / Indie / NC
	FString AssetType = TEXT("Full");
	if (bAssetLimitedCommercial)
		AssetType = TEXT("Limited Commercial (LC)");
	else if (bAssetNonCommercial)
		AssetType = TEXT("Non Commercial (NC)");

	OutTags.Add(FAssetRegistryTag("Asset Type", AssetType, FAssetRegistryTag::TT_Alphabetical));

	Super::GetAssetRegistryTags(OutTags);
}

bool
UT2HoudiniAsset::IsAssetLimitedCommercial() const
{
	return bAssetLimitedCommercial;
}

bool
UT2HoudiniAsset::IsAssetNonCommercial() const
{
	return bAssetNonCommercial;
}

bool
UT2HoudiniAsset::IsExpandedHDA() const
{
	return bAssetExpanded;
}
