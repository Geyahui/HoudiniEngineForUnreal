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

#include "T2HoudiniAssetFactory.h"

#include "HoudiniEngineEditorPrivatePCH.h"
#include "T2HoudiniAsset.h"

#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UT2HoudiniAssetFactory::UT2HoudiniAssetFactory(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	// This factory is responsible for manufacturing HoudiniEngine assets.
	SupportedClass = UT2HoudiniAsset::StaticClass();

	// This factory does not manufacture new objects from scratch.
	bCreateNew = false;

	// This factory will not open the editor for each new object.
	bEditAfterNew = false;

	// This factory will import objects from files.
	bEditorImport = true;

	// Factory does not import objects from text.
	bText = false;

	// Add supported formats.
	Formats.Add(TEXT("otl;Houdini Engine Asset"));
	Formats.Add(TEXT("otllc;Houdini Engine Limited Commercial Asset"));
	Formats.Add(TEXT("otlnc;Houdini Engine Non-Commercial Asset"));
	Formats.Add(TEXT("hda;Houdini Engine Asset"));
	Formats.Add(TEXT("hdalc;Houdini Engine Limited Commercial Asset"));
	Formats.Add(TEXT("hdanc;Houdini Engine Non-Commercial Asset"));
	Formats.Add(TEXT("hdalibrary;Houdini Engine Expanded Asset"));
}
bool UT2HoudiniAssetFactory::FactoryCanImport(const FString& Filename)
{
	// 1. 首先调用父类方法，确认后缀匹配
	// if (!Super::FactoryCanImport(Filename)) {
	// 	return false;
	// }

	// // 2. 读取文件内容，检查特殊标记
	// FString FileContent;
	// if (FFileHelper::LoadFileToString(FileContent, *Filename)) {
	// 	// 检查是否存在插件A的标记（例如 JSON 中的 "generator": "PluginA"）
	// 	if (FileContent.Contains(TEXT("\"generator\": \"PluginA\""))) {
	// 		return true;
	// 	}
	// }

	return true;
}

bool
UT2HoudiniAssetFactory::DoesSupportClass(UClass * Class)
{
	return Class == SupportedClass;
}

FText
UT2HoudiniAssetFactory::GetDisplayName() const
{
	return LOCTEXT("HoudiniAssetFactoryDescription", "Houdini Engine Asset");
}

UObject *
UT2HoudiniAssetFactory::FactoryCreateBinary(
	UClass * InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject * Context, const TCHAR * Type, const uint8 *& Buffer,
	const uint8 * BufferEnd, FFeedbackContext * Warn )
{
	// Broadcast notification that a new asset is being imported.
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Type);

	// Create a new asset.
	UT2HoudiniAsset * HoudiniAsset = NewObject< UT2HoudiniAsset >(InParent, InName, Flags);
	HoudiniAsset->CreateAsset(Buffer, BufferEnd, UFactory::GetCurrentFilename());

	// Create reimport information.
	UAssetImportData * AssetImportData = HoudiniAsset->AssetImportData;
	if (!AssetImportData)
	{
		AssetImportData = NewObject< UAssetImportData >(HoudiniAsset, UAssetImportData::StaticClass());
		HoudiniAsset->AssetImportData = AssetImportData;
	}

	AssetImportData->Update(UFactory::GetCurrentFilename());

	// Broadcast notification that the new asset has been imported.
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, HoudiniAsset);

	return HoudiniAsset;
}

UObject*
UT2HoudiniAssetFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	// "houdini.hdalibrary" files (expanded hda / hda folder) need a special treatment,
	// but ".hda" files can be loaded normally
	FString FileExtension = FPaths::GetExtension(Filename);
	if (FileExtension.Compare(TEXT("hdalibrary"), ESearchCase::IgnoreCase) != 0)
		return Super::FactoryCreateFile(InClass, InParent, InName, Flags, Filename, Parms, Warn, bOutOperationCanceled);

	// Make sure the file name is sections.list
	FString NameOfFile = FPaths::GetBaseFilename(Filename);
	if (NameOfFile.Compare(TEXT("houdini"), ESearchCase::IgnoreCase) != 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to load file '%s'. File is not a valid extended HDA."), *Filename);
		return nullptr;
	}

	// Make sure that the proper .list file is loaded
	FString PathToFile = FPaths::GetPath(Filename);
	if (PathToFile.Find(TEXT(".hda")) != (PathToFile.Len() - 4))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to load file '%s'. File is not a valid extended HDA."), *Filename);
		return nullptr;
	}

	FString NewFilename = PathToFile;
	FString NewFileNameNoHDA = FPaths::GetBaseFilename(PathToFile);
	FName NewIname = FName(*NewFileNameNoHDA);
	FString NewFileExtension = FPaths::GetExtension(NewFilename);

	// load as binary
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *Filename))
	{
		HOUDINI_LOG_ERROR(TEXT("Failed to load file '%s' to array"), *Filename);
		return nullptr;
	}

	Data.Add(0);
	ParseParms(Parms);
	const uint8* Ptr = &Data[0];

	return FactoryCreateBinary(InClass, InParent, NewIname, Flags, nullptr, *NewFileExtension, Ptr, Ptr + Data.Num() - 1, Warn);
}

bool
UT2HoudiniAssetFactory::CanReimport(UObject * Obj, TArray< FString > & OutFilenames)
{
	UT2HoudiniAsset * HoudiniAsset = Cast<UT2HoudiniAsset>(Obj);
	if (HoudiniAsset)
	{
		UAssetImportData * AssetImportData = HoudiniAsset->AssetImportData;
		if (AssetImportData)
			OutFilenames.Add(AssetImportData->GetFirstFilename());
		else
			OutFilenames.Add(TEXT(""));

		return true;
	}

	return false;
}

void
UT2HoudiniAssetFactory::SetReimportPaths(UObject * Obj, const TArray< FString > & NewReimportPaths)
{
	UT2HoudiniAsset * HoudiniAsset = Cast< UT2HoudiniAsset >(Obj);
	if (HoudiniAsset && (1 == NewReimportPaths.Num()))
		HoudiniAsset->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
}

EReimportResult::Type
UT2HoudiniAssetFactory::Reimport(UObject * Obj)
{
	UT2HoudiniAsset * HoudiniAsset = Cast< UT2HoudiniAsset >(Obj);
	if (HoudiniAsset && HoudiniAsset->AssetImportData)
	{
		// Make sure file is valid and exists.
		const FString & Filename = HoudiniAsset->AssetImportData->GetFirstFilename();

		if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
			return EReimportResult::Failed;

		if (UFactory::StaticImportObject(
			HoudiniAsset->GetClass(), HoudiniAsset->GetOuter(), *HoudiniAsset->GetName(),
			RF_Public | RF_Standalone, *Filename, NULL, this))
		{
			HOUDINI_LOG_MESSAGE(TEXT("Houdini Asset reimported successfully."));

			if (HoudiniAsset->GetOuter())
				HoudiniAsset->GetOuter()->MarkPackageDirty();
			else
				HoudiniAsset->MarkPackageDirty();

			return EReimportResult::Succeeded;
		}
	}

	HOUDINI_LOG_MESSAGE(TEXT("Houdini Asset reimport has failed."));
	return EReimportResult::Failed;
}

#undef LOCTEXT_NAMESPACE