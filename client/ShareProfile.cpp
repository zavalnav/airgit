/*
 * Copyright (C) 2012-2013 AirDC++ Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"

#include "ShareProfile.h"
#include "TimerManager.h"
#include "FilteredFile.h"
#include "Streams.h"
#include "SimpleXML.h"
#include "BZUtils.h"
#include "ClientManager.h"

namespace dcpp {

FileList::FileList(ProfileToken aProfile) : profile(aProfile), xmlDirty(true), forceXmlRefresh(true), lastXmlUpdate(0), listN(0), isSavedSuccessfully(false) { }

string FileList::getFileName() {
	return Util::getPath(Util::PATH_USER_CONFIG) + "files_" + Util::toString(profile) + "_" + Util::toString(listN) + ".xml.bz2";
}

bool FileList::allowGenerateNew(bool forced) {
	bool dirty = (forced && xmlDirty) || forceXmlRefresh || (xmlDirty && (lastXmlUpdate + 15 * 60 * 1000 < GET_TICK()));
	if (!dirty) {
		return false;
	}

	return true;
}

void FileList::generationFinished(bool failed) {
	xmlDirty = false;
	forceXmlRefresh = false;
	lastXmlUpdate = GET_TICK();
	if (!failed)
		listN++;
}

void FileList::saveList() {
	if(bzXmlRef.get()) {
		bzXmlRef.reset();
	}

	bzXmlRef = unique_ptr<File>(new File(getFileName(), File::READ, File::OPEN, false));
	bzXmlListLen = File::getSize(getFileName());

	//cleanup old filelists we failed to delete before due to uploading them.
	StringList lists = File::findFiles(Util::getPath(Util::PATH_USER_CONFIG), "files_" + Util::toString(profile) + "?*.xml.bz2");
	boost::for_each(lists, File::deleteFile);
}

ShareProfileInfo::ShareProfileInfo(const string& aName, ProfileToken aToken /*rand*/, State aState /*STATE_NORMAL*/) : name(aName), token(aToken), state(aState), isDefault(false) {}

string ShareProfileInfo::getDisplayName() const {
	string ret = name;
	if (isDefault) {
		ret += " (" + STRING(DEFAULT) + ")";
	}
	return ret;
}

ShareProfile::ShareProfile(const string& aName, ProfileToken aToken) : token(aToken), plainName(aName), profileList(new FileList(aToken)), sharedFiles(0), shareSize(0), profileInfoDirty(true) { }

ShareProfile::~ShareProfile() {
	delete profileList;
}

string ShareProfile::getDisplayName() const {
	string ret = plainName;
	if (token == SETTING(DEFAULT_SP)) {
		ret += " (" + STRING(DEFAULT) + ")";
	}
	return ret;
}

FileList* ShareProfile::generateProfileList() {
	profileList = new FileList(token);
	return profileList;
}

} //dcpp