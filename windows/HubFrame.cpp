/*
 * Copyright (C) 2001-2015 Jacek Sieka, arnetheduck on gmail point com
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

#include "stdafx.h"
#include "Resource.h"

#include "HubFrame.h"
#include "LineDlg.h"
#include "SearchFrm.h"
#include "PrivateFrame.h"
#include "TextFrame.h"
#include "ResourceLoader.h"
#include "MainFrm.h"

#include <airdcpp/ColorSettings.h>
#include <airdcpp/HighlightManager.h>
#include <airdcpp/DirectoryListingManager.h>
#include <airdcpp/Message.h>
#include <airdcpp/QueueManager.h>
#include <airdcpp/ShareManager.h>
#include <airdcpp/UploadManager.h>
#include <airdcpp/Util.h>
#include <airdcpp/FavoriteManager.h>
#include <airdcpp/LogManager.h>
#include <airdcpp/SettingsManager.h>
#include <airdcpp/Wildcards.h>
#include <airdcpp/Localization.h>
#include <airdcpp/GeoManager.h>

HubFrame::FrameMap HubFrame::frames;
bool HubFrame::shutdown = false;

int HubFrame::columnSizes[] = { 100, 75, 75, 75, 100, 75, 130, 130, 100, 50, 40, 40, 40, 40, 40, 300 };
int HubFrame::columnIndexes[] = { OnlineUser::COLUMN_NICK, OnlineUser::COLUMN_SHARED, OnlineUser::COLUMN_EXACT_SHARED,
	OnlineUser::COLUMN_DESCRIPTION, OnlineUser::COLUMN_TAG,	OnlineUser::COLUMN_ULSPEED, OnlineUser::COLUMN_DLSPEED, OnlineUser::COLUMN_IP4, OnlineUser::COLUMN_IP6, OnlineUser::COLUMN_EMAIL,
	OnlineUser::COLUMN_VERSION, OnlineUser::COLUMN_MODE4, OnlineUser::COLUMN_MODE6, OnlineUser::COLUMN_FILES, OnlineUser::COLUMN_HUBS, OnlineUser::COLUMN_SLOTS, OnlineUser::COLUMN_CID };

ResourceManager::Strings HubFrame::columnNames[] = { ResourceManager::NICK, ResourceManager::SHARED, ResourceManager::EXACT_SHARED, 
	ResourceManager::DESCRIPTION, ResourceManager::TAG, ResourceManager::SETCZDC_UPLOAD_SPEED, ResourceManager::SETCZDC_DOWNLOAD_SPEED, ResourceManager::IP_V4, ResourceManager::IP_V6, ResourceManager::EMAIL,
	ResourceManager::VERSION, ResourceManager::MODE_V4, ResourceManager::MODE_V6, ResourceManager::SHARED_FILES, ResourceManager::HUBS, ResourceManager::SLOTS, ResourceManager::CID };

static ColumnType columnTypes [] = { COLUMN_TEXT, COLUMN_SIZE, COLUMN_SIZE, 
	COLUMN_TEXT, COLUMN_TEXT, COLUMN_SPEED, COLUMN_SPEED, COLUMN_TEXT, COLUMN_TEXT, COLUMN_TEXT, 
	COLUMN_TEXT, COLUMN_TEXT, COLUMN_TEXT, COLUMN_NUMERIC_OTHER, COLUMN_NUMERIC_OTHER, COLUMN_NUMERIC_OTHER, COLUMN_TEXT };

LRESULT HubFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);

	ctrlClient.setClient(client);
	init(m_hWnd, rcDefault);
	ctrlMessageContainer.SubclassWindow(ctrlMessage.m_hWnd);
	ctrlClientContainer.SubclassWindow(ctrlClient.m_hWnd);
	ctrlStatusContainer.SubclassWindow(ctrlStatus.m_hWnd);

	ctrlUsers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | 
		WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_USERS);
	ctrlUsers.SetExtendedListViewStyle(LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);

	SetSplitterPanes(ctrlClient.m_hWnd, ctrlUsers.m_hWnd, false);
	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
	
	if(hubchatusersplit) {
		m_nProportionalPos = hubchatusersplit;
	} else {
		m_nProportionalPos = 7500;
	}

	ctrlShowUsers.Create(ctrlStatus.m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ctrlShowUsers.SetButtonStyle(BS_AUTOCHECKBOX, false);
	ctrlShowUsers.SetFont(WinUtil::systemFont);
	ctrlShowUsers.SetCheck(showUsers ? BST_CHECKED : BST_UNCHECKED);
	ctrlShowUsersContainer.SubclassWindow(ctrlShowUsers.m_hWnd);

	iSecure = ResourceLoader::loadIcon(IDI_SECURE, 16);

	auto fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(Text::fromT(server));
	if(fhe) {
		WinUtil::splitTokens(columnIndexes, fhe->getHeaderOrder(), OnlineUser::COLUMN_LAST);
		WinUtil::splitTokens(columnSizes, fhe->getHeaderWidths(), OnlineUser::COLUMN_LAST);
	} else {
		WinUtil::splitTokens(columnIndexes, SETTING(HUBFRAME_ORDER), OnlineUser::COLUMN_LAST);
		WinUtil::splitTokens(columnSizes, SETTING(HUBFRAME_WIDTHS), OnlineUser::COLUMN_LAST);                           
	}
    	
	for(uint8_t j = 0; j < OnlineUser::COLUMN_LAST; ++j) {
		int fmt = (j == OnlineUser::COLUMN_SHARED || j == OnlineUser::COLUMN_EXACT_SHARED || j == OnlineUser::COLUMN_SLOTS) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlUsers.InsertColumn(j, CTSTRING_I(columnNames[j]), fmt, columnSizes[j], j, columnTypes[j]);
	}

	ctrlUsers.addCopyHandler(OnlineUser::COLUMN_IP4, &ColumnInfo::filterCountry);
	ctrlUsers.addCopyHandler(OnlineUser::COLUMN_IP6, &ColumnInfo::filterCountry);

	filter.addFilterBox(m_hWnd);
	filter.addColumnBox(m_hWnd, ctrlUsers.getColumnList());
	filter.addMethodBox(m_hWnd);

	ctrlUsers.setColumnOrderArray(OnlineUser::COLUMN_LAST, columnIndexes);
	
	if(fhe) {
		ctrlUsers.setVisible(fhe->getHeaderVisible());
    } else {
	    ctrlUsers.setVisible(SETTING(HUBFRAME_VISIBLE));
    }
	
	ctrlUsers.SetFont(WinUtil::listViewFont); //this will also change the columns font
	ctrlUsers.SetBkColor(WinUtil::bgColor);
	ctrlUsers.SetTextBkColor(WinUtil::bgColor);
	ctrlUsers.SetTextColor(WinUtil::textColor);
	ctrlUsers.setFlickerFree(WinUtil::bgBrush);
	ctrlUsers.setSortColumn(OnlineUser::COLUMN_NICK);
	ctrlUsers.SetImageList(ResourceLoader::getUserImages(), LVSIL_SMALL);

	CToolInfo ti_1(TTF_SUBCLASS, ctrlStatus.m_hWnd, 0 + POPUP_UID, 0, LPSTR_TEXTCALLBACK);
	CToolInfo ti_2(TTF_SUBCLASS, ctrlStatus.m_hWnd, 1 + POPUP_UID, 0, LPSTR_TEXTCALLBACK);
	ctrlTooltips.AddTool(&ti_1);
	ctrlTooltips.AddTool(&ti_2);

	CToolInfo ti_3(TTF_SUBCLASS, ctrlShowUsers.m_hWnd);
	ti_3.cbSize = sizeof(CToolInfo);
	ti_3.lpszText = (LPWSTR)CTSTRING(SHOW_USERLIST);
	ctrlTooltips.AddTool(&ti_3);
	ctrlTooltips.SetDelayTime(TTDT_AUTOPOP, 15000);
	ctrlTooltips.Activate(TRUE);


	WinUtil::SetIcon(m_hWnd, IDI_HUB);

	if(fhe){
		//retrieve window position
		CRect rc(fhe->getLeft(), fhe->getTop(), fhe->getRight(), fhe->getBottom());
		
		//check that we have a window position stored
		if(! (rc.top == 0 && rc.bottom == 0 && rc.left == 0 && rc.right == 0) )
			MoveWindow(rc, TRUE);
	}
	
	bHandled = FALSE;

	client->addListener(this);
	client->connect();

	FavoriteManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	MessageManager::getInstance()->addListener(this);

	::SetTimer(m_hWnd, 0, 500, 0);
	return 1;
}

void HubFrame::openWindow(const tstring& aServer) {
	auto i = frames.find(aServer);
	if(i == frames.end()) {
		HubFrame* frm = new HubFrame(aServer);
		frames[aServer] = frm;
		frm->CreateEx(WinUtil::mdiClient, frm->rcDefault);
	}
}

HubFrame::~HubFrame() {
}

HubFrame::HubFrame(const tstring& aServer) : 
		waitingForPW(false), extraSort(false), server(aServer), closed(false), forceClose(false),
		showUsers(SETTING(GET_USER_INFO)), updateUsers(false), resort(false), countType(Client::COUNT_NORMAL),
		timeStamps(SETTING(TIME_STAMPS)),
		hubchatusersplit(0),
		ctrlShowUsersContainer(WC_BUTTON, this, SHOW_USERS),
		ctrlMessageContainer(WC_EDIT, this, EDIT_MESSAGE_MAP),
		ctrlClientContainer(WC_EDIT, this, EDIT_MESSAGE_MAP),
		ctrlStatusContainer(STATUSCLASSNAME, this, STATUS_MSG),
		filter(OnlineUser::COLUMN_LAST, [this] { updateUserList(); updateUsers = true;}),
		statusDirty(true)
{
	client = ClientManager::getInstance()->getClient(Text::fromT(server));
	dcassert(client);

	auto fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(Text::fromT(server));
	if(fhe) {
		hubchatusersplit = fhe->getChatUserSplit();
		showUsers = fhe->getUserListState();
	} else {
		showUsers = SETTING(GET_USER_INFO);
	}
		
	memset(statusSizes, 0, sizeof(statusSizes));
}

bool HubFrame::sendMessage(const tstring& aMessage, string& error_, bool isThirdPerson) {
	return client->hubMessage(Text::fromT(aMessage), error_, isThirdPerson);
}

bool HubFrame::checkFrameCommand(tstring& cmd, tstring& param, tstring& /*message*/, tstring& status, bool& /*thirdPerson*/) {	
	if(stricmp(cmd.c_str(), _T("join"))==0) {
		if(!param.empty()) {
			if(SETTING(JOIN_OPEN_NEW_WINDOW)) {
				RecentHubEntryPtr r = new RecentHubEntry(Text::fromT(param));
				WinUtil::connectHub(r, SETTING(DEFAULT_SP));
			} else {
				BOOL whatever = FALSE;
				onFollow(0, 0, 0, whatever);
			}
		} else {
			status = TSTRING(SPECIFY_SERVER);
		}
	} else if(stricmp(cmd.c_str(), _T("ts")) == 0) {
		timeStamps = !timeStamps;
		if(timeStamps) {
			status = TSTRING(TIMESTAMPS_ENABLED);
		} else {
			status = TSTRING(TIMESTAMPS_DISABLED);
		}
	} else if( (stricmp(cmd.c_str(), _T("password")) == 0) && waitingForPW ) {
		client->password(Text::fromT(param));
		waitingForPW = false;
	} else if( stricmp(cmd.c_str(), _T("showjoins")) == 0 ) {
		if(client->changeBoolHubSetting(HubSettings::ShowJoins)) {
			status = TSTRING(JOIN_SHOWING_ON);
		} else {
			status = TSTRING(JOIN_SHOWING_OFF);
		}
	} else if( stricmp(cmd.c_str(), _T("favshowjoins")) == 0 ) {
		if(client->changeBoolHubSetting(HubSettings::FavShowJoins)) {
			status = TSTRING(FAV_JOIN_SHOWING_ON);
		} else {
			status = TSTRING(FAV_JOIN_SHOWING_OFF);
		}
	} else if(stricmp(cmd.c_str(), _T("close")) == 0) {
		PostMessage(WM_CLOSE);
	} else if(stricmp(cmd.c_str(), _T("userlist")) == 0) {
		ctrlShowUsers.SetCheck(showUsers ? BST_UNCHECKED : BST_CHECKED);
	} else if((stricmp(cmd.c_str(), _T("favorite")) == 0) || (stricmp(cmd.c_str(), _T("fav")) == 0)) {
		addAsFavorite();
	} else if((stricmp(cmd.c_str(), _T("removefavorite")) == 0) || (stricmp(cmd.c_str(), _T("removefav")) == 0)) {
		removeFavoriteHub();
	} else if(stricmp(cmd.c_str(), _T("getlist")) == 0){
		if(!param.empty() ){
			OnlineUserPtr ui = client->findUser(Text::fromT(param));
			if(ui) {
				ui->getList();
			}
		}
	} else if(stricmp(cmd.c_str(), _T("log")) == 0) {
		WinUtil::openFile(Text::toT(getLogPath(stricmp(param.c_str(), _T("status")) == 0)));
	} else if(stricmp(cmd.c_str(), _T("help")) == 0) {
		status = _T("*** ") + ChatFrameBase::commands + _T("Additional commands for the hub tab: /join <hub-ip>, /ts, /showjoins, /favshowjoins, /close, /userlist, /favorite, /pm <user> [message], /getlist <user>, /removefavorite");
	} else if(stricmp(cmd.c_str(), _T("pm")) == 0) {
		string::size_type j = param.find(_T(' '));
		if(j != string::npos) {
			tstring nick = param.substr(0, j);
			const OnlineUserPtr ui = client->findUser(Text::fromT(nick));

			if(ui) {
				PrivateFrame::openWindow(HintedUser(ui->getUser(), client->getHubUrl()));
			}
		} else if(!param.empty()) {
			const OnlineUserPtr ui = client->findUser(Text::fromT(param));
			if(ui) {
				PrivateFrame::openWindow(HintedUser(ui->getUser(), client->getHubUrl()));
			}
		}
	} else if(stricmp(cmd.c_str(), _T("topic")) == 0) {
		addLine(_T("*** ") + Text::toT(client->getHubDescription()));
	} else if(stricmp(cmd.c_str(), _T("ctopic")) == 0) {
		openLinksInTopic();
	} else if (stricmp(cmd.c_str(), _T("allow")) == 0) {
		client->allowUntrustedConnect();
	} else {
		return false;
	}

	return true;
}

struct CompareItems {
	CompareItems(uint8_t aCol) : col(aCol) { }
	bool operator()(const OnlineUser& a, const OnlineUser& b) const {
		return OnlineUser::compareItems(&a, &b, col) < 0;
	}
	const uint8_t col;
};

void HubFrame::addAsFavorite() {
	if (client->saveFavorite()) {
		addStatus(TSTRING(FAVORITE_HUB_ADDED), LogMessage::SEV_INFO, WinUtil::m_ChatTextSystem);
	} else {
		addStatus(TSTRING(FAVORITE_HUB_ALREADY_EXISTS), LogMessage::SEV_ERROR, WinUtil::m_ChatTextSystem);
	}
}

void HubFrame::removeFavoriteHub() {
	auto removeHub = FavoriteManager::getInstance()->getFavoriteHubEntry(client->getHubUrl());
	if(removeHub) {
		FavoriteManager::getInstance()->removeFavoriteHub(removeHub->getToken());
		addStatus(TSTRING(FAVORITE_HUB_REMOVED), LogMessage::SEV_INFO, WinUtil::m_ChatTextSystem);
	} else {
		addStatus(TSTRING(FAVORITE_HUB_DOES_NOT_EXIST), LogMessage::SEV_ERROR, WinUtil::m_ChatTextSystem);
	}
}

LRESULT HubFrame::onCopyHubInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
    if(client->isConnected()) {
        string sCopy;

		switch (wID) {
			case IDC_COPY_HUBNAME:
				sCopy += client->getHubName();
				break;
			case IDC_COPY_HUBADDRESS:
				sCopy += client->getHubUrl();
				break;
		}

		if (!sCopy.empty())
			WinUtil::setClipboard(Text::toT(sCopy));
    }
	return 0;
}

LRESULT HubFrame::onCopyUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	tstring sCopy;

	int sel = -1;
	while((sel = ctrlUsers.GetNextItem(sel, LVNI_SELECTED)) != -1) {
		const OnlineUserPtr ou = ctrlUsers.getItemData(sel);
	
		if(!sCopy.empty())
			sCopy += _T("\r\n");

		sCopy += ou->getText(static_cast<uint8_t>(wID - IDC_COPY), true);
	}
	if (!sCopy.empty())
		WinUtil::setClipboard(sCopy);

	return 0;
}
LRESULT HubFrame::onCopyAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	tstring sCopy;

	int sel = -1;
	while((sel = ctrlUsers.GetNextItem(sel, LVNI_SELECTED)) != -1) {
		if (!sCopy.empty())
			sCopy += _T("\r\n\r\n");
		sCopy += ctrlUsers.GetColumnTexts(sel);
	}

	if (!sCopy.empty())
		WinUtil::setClipboard(sCopy);

	return 0;
}

LRESULT HubFrame::onDoubleClickUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/) {
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	if(item->iItem != -1 && (ctrlUsers.getItemData(item->iItem)->getUser() != ClientManager::getInstance()->getMe())) {
	    switch(SETTING(USERLIST_DBLCLICK)) {
		    case 0:
				ctrlUsers.getItemData(item->iItem)->getList();
		        break;
		    case 1: {
				tstring sUser = Text::toT(ctrlUsers.getItemData(item->iItem)->getIdentity().getNick());
	            int iSelBegin, iSelEnd;
	            ctrlMessage.GetSel(iSelBegin, iSelEnd);

	            if((iSelBegin == 0) && (iSelEnd == 0)) {
					sUser += _T(": ");
					if(ctrlMessage.GetWindowTextLength() == 0) {
			            ctrlMessage.SetWindowText(sUser.c_str());
			            ctrlMessage.SetFocus();
			            ctrlMessage.SetSel(ctrlMessage.GetWindowTextLength(), ctrlMessage.GetWindowTextLength());
                    } else {
			            ctrlMessage.ReplaceSel(sUser.c_str());
			            ctrlMessage.SetFocus();
                    }
				} else {
					sUser += _T(" ");
                    ctrlMessage.ReplaceSel(sUser.c_str());
                    ctrlMessage.SetFocus();
	            }
				break;
		    }    
		    case 2:
				ctrlUsers.getItemData(item->iItem)->pm();
		        break;
		    case 3:
		        ctrlUsers.getItemData(item->iItem)->matchQueue();
		        break;
		    case 4:
		        ctrlUsers.getItemData(item->iItem)->grant();
		        break;
		    case 5:
		        ctrlUsers.getItemData(item->iItem)->handleFav();
		        break;
			case 6:
				ctrlUsers.getItemData(item->iItem)->browseList();
				break;
		}	
	}
	return 0;
}

bool HubFrame::updateUser(const UserTask& u) {
	if(!showUsers) return false;
	
	if(!u.onlineUser->isInList) {
		u.onlineUser->update(-1);

		if(!u.onlineUser->isHidden()) {
			u.onlineUser->inc();
			ctrlUsers.insertItem(u.onlineUser.get(), u.onlineUser->getImageIndex());
		}

		if(!filter.empty())
			updateUserList(u.onlineUser);
		return true;
	} else {
		const int pos = ctrlUsers.findItem(u.onlineUser.get());

		if(pos != -1) {
			TCHAR buf[255];
			ListView_GetItemText(ctrlUsers, pos, ctrlUsers.getSortColumn(), buf, 255);
			
			resort = u.onlineUser->update(ctrlUsers.getSortColumn(), buf) || resort;
			if(u.onlineUser->isHidden()) {
				ctrlUsers.DeleteItem(pos);
				u.onlineUser->dec();				
			} else {
				ctrlUsers.updateItem(pos);
				ctrlUsers.SetItem(pos, 0, LVIF_IMAGE, NULL, u.onlineUser->getImageIndex(), 0, 0, NULL);
			}
		}

		u.onlineUser->getIdentity().set("WO", u.onlineUser->getIdentity().isOp() ? "1" : Util::emptyString);
		updateUserList(u.onlineUser);
		return false;
	}
}

void HubFrame::removeUser(const OnlineUserPtr& aUser) {
	if(!showUsers) return;
	
	if(!aUser->isHidden()) {
		int i = ctrlUsers.findItem(aUser.get());
		if(i != -1) {
			ctrlUsers.DeleteItem(i);
			aUser->dec();
		}
	}
}

void HubFrame::onChatMessage(const ChatMessagePtr& msg) {
	addLine(msg->getFrom()->getIdentity(), Text::toT(msg->format()), WinUtil::m_ChatTextGeneral);
	if (client->get(HubSettings::ChatNotify)) {
		MainFrame::getMainFrame()->onChatMessage(false);
	}
}

void HubFrame::onDisconnected(const string&) {
	setDisconnected(true);
	wentoffline = true;
	setTabIcons();

	clearTaskList();
	clearUserList();

	if ((!SETTING(SOUND_HUBDISCON).empty()) && (!SETTING(SOUNDS_DISABLED)))
		WinUtil::playSound(Text::toT(SETTING(SOUND_HUBDISCON)));

	if(SETTING(POPUP_HUB_DISCONNECTED)) {
		WinUtil::showPopup(Text::toT(client->getAddress()), TSTRING(DISCONNECTED));
	}
}

void HubFrame::onConnected() {
	wentoffline = false;
	setDisconnected(false);
	setTabIcons();

	if (client->isSecure()) {
		ctrlStatus.SetIcon(1, iSecure);
		statusSizes[0] = 16 + 8;
		tstring sslInfo = Text::toT(client->getEncryptionInfo());
		if (!sslInfo.empty())
			cipherPopupTxt = (client->isTrusted() ? _T("[S] ") : _T("[U] ")) + sslInfo;
	}	

	if(SETTING(POPUP_HUB_CONNECTED)) {
		WinUtil::showPopup(Text::toT(client->getAddress()), TSTRING(CONNECTED));
	}

	if ((!SETTING(SOUND_HUBCON).empty()) && (!SETTING(SOUNDS_DISABLED)))
		WinUtil::playSound(Text::toT(SETTING(SOUND_HUBCON)));
}

void HubFrame::setWindowTitle(const string& aTitle) {
	SetWindowText(Text::toT(aTitle).c_str());
	MDIRefreshMenu();
}

void HubFrame::execTasks() {
	TaskQueue::List tl;
	tasks.get(tl);

	ctrlUsers.SetRedraw(FALSE);

	for(auto& t: tl) {
		if(t.first == UPDATE_USER) {
			updateUser(static_cast<UserTask&>(*t.second));
		} else if(t.first == UPDATE_USER_JOIN) {
			UserTask& u = static_cast<UserTask&>(*t.second);
			if(updateUser(u)) {
				bool isFavorite = u.onlineUser->getUser()->isFavorite();
				if (isFavorite && (!SETTING(SOUND_FAVUSER).empty()) && (!SETTING(SOUNDS_DISABLED)))
					WinUtil::playSound(Text::toT(SETTING(SOUND_FAVUSER)));

				if(isFavorite && SETTING(POPUP_FAVORITE_CONNECTED)) {
					WinUtil::showPopup(Text::toT(u.onlineUser->getIdentity().getNick() + " - " + client->getHubName()), TSTRING(FAVUSER_ONLINE));
				}

				if (!u.onlineUser->isHidden() && client->get(HubSettings::ShowJoins) || (client->get(HubSettings::FavShowJoins) && isFavorite)) {
				 	addLine(_T("*** ") + TSTRING(JOINS) + _T(": ") + Text::toT(u.onlineUser->getIdentity().getNick()), WinUtil::m_ChatTextSystem, SETTING(HUB_BOLD_TABS));
				}	
			}
		} else if(t.first == REMOVE_USER) {
			const UserTask& u = static_cast<UserTask&>(*t.second);
			removeUser(u.onlineUser);

			if (!u.onlineUser->isHidden() && client->get(HubSettings::ShowJoins) || (client->get(HubSettings::FavShowJoins) && u.onlineUser->getUser()->isFavorite())) {
				addLine(Text::toT("*** " + STRING(PARTS) + ": " + u.onlineUser->getIdentity().getNick()), WinUtil::m_ChatTextSystem, SETTING(HUB_BOLD_TABS));
			}
		}
	}
	
	if(resort && showUsers) {
		ctrlUsers.resort();
		resort = false;
	}

	ctrlUsers.SetRedraw(TRUE);
}

void HubFrame::onPassword() {
	if(!SETTING(PROMPT_PASSWORD)) {
		ctrlMessage.SetWindowText(_T("/password "));
		ctrlMessage.SetFocus();
		ctrlMessage.SetSel(10, 10);
		waitingForPW = true;
	} else {
		LineDlg linePwd;
		linePwd.title = CTSTRING(ENTER_PASSWORD);
		linePwd.description = CTSTRING(ENTER_PASSWORD);
		linePwd.password = true;
		if(linePwd.DoModal(m_hWnd) == IDOK) {
			client->password(Text::fromT(linePwd.line));
			waitingForPW = false;
		} else {
			client->disconnect(true);
		}
	}
}

void HubFrame::onUpdateTabIcons() {
	if (client->getCountType() != countType) {
		countType = client->getCountType();
		callAsync([=] { setTabIcons(); });
	}
}

void HubFrame::setTabIcons() {
	if (wentoffline)
		tabIcon = ResourceLoader::getHubImages().GetIcon(3);
	else if (countType == Client::COUNT_OP)
		tabIcon = ResourceLoader::getHubImages().GetIcon(2);
	else if (countType == Client::COUNT_REGISTERED)
		tabIcon = ResourceLoader::getHubImages().GetIcon(1);
	else
		tabIcon = ResourceLoader::getHubImages().GetIcon(0);

	setIcon(tabIcon);
}

void HubFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */) {
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if(ctrlStatus.IsWindow()) {
		CRect sr;
		int w[6];
		ctrlStatus.GetClientRect(sr);

		w[0] = sr.right - statusSizes[0] - statusSizes[1] - statusSizes[2] - statusSizes[3] - 16;
		w[1] = w[0] + statusSizes[0];
		w[2] = w[1] + statusSizes[1];
		w[3] = w[2] + statusSizes[2];
		w[4] = w[3] + statusSizes[3];
		w[5] = w[4] + 16;
		
		ctrlStatus.SetParts(6, w);
		ctrlTooltips.SetMaxTipWidth(w[0]);

		// Strange, can't get the correct width of the last field...
		ctrlStatus.GetRect(4, sr);
		sr.left = sr.right;
		sr.right = sr.left + 16;
		ctrlShowUsers.MoveWindow(sr);

		CRect r;
		ctrlStatus.GetRect(0, r);
		ctrlTooltips.SetToolRect(ctrlStatus.m_hWnd, 0 + POPUP_UID, r);
		ctrlStatus.GetRect(1, r);
		ctrlTooltips.SetToolRect(ctrlStatus.m_hWnd, 1 + POPUP_UID, r);
	}
		
	int h = WinUtil::fontHeight + 4;

	const int maxLines = resizePressed && (SETTING(MAX_RESIZE_LINES) <= 1) ? 2 : SETTING(MAX_RESIZE_LINES);

	if((maxLines != 1) && lineCount != 0) {
		if(lineCount < maxLines) {
			h = WinUtil::fontHeight * lineCount + 4;
		} else {
			h = WinUtil::fontHeight * maxLines + 4;
		}
	} 

	CRect rc = rect;
	rc.bottom -= h + 10;
	if(!showUsers) {
		if(GetSinglePaneMode() == SPLIT_PANE_NONE)
			SetSinglePaneMode(SPLIT_PANE_LEFT);
	} else {
		if(GetSinglePaneMode() != SPLIT_PANE_NONE)
			SetSinglePaneMode(SPLIT_PANE_NONE);
	}
	SetSplitterRect(rc);

	int buttonsize = 0;
	
	if(ctrlEmoticons.IsWindow())
		buttonsize +=26;

	if(ctrlMagnet.IsWindow())
		buttonsize += 26;

	if(ctrlResize.IsWindow())
		buttonsize += 26;

	rc = rect;
	rc.bottom -= 2;
	rc.top = rc.bottom - h - 5;
	rc.left +=2;
	rc.right -= (showUsers ? 320 : 0) + buttonsize;
	ctrlMessage.MoveWindow(rc);

//ApexDC
	if(h != (WinUtil::fontHeight + 4)) {
		rc.bottom -= h - (WinUtil::fontHeight + 4);
	}
//end

	if(ctrlResize.IsWindow()) {
		//resize lines button
		rc.left = rc.right + 2;
		rc.right += 24;
		ctrlResize.MoveWindow(rc);
	}

	if(ctrlEmoticons.IsWindow()) {
		rc.left = rc.right + 2;
		rc.right += 24;
		ctrlEmoticons.MoveWindow(rc);
	}
	
	if(ctrlMagnet.IsWindow()){
		//magnet button
		rc.left = rc.right + 2;
		rc.right += 24;
		ctrlMagnet.MoveWindow(rc);
	}

	if(showUsers){
		rc.left = rc.right + 2;
		rc.right = rc.left + 116;
		filter.getFilterBox().MoveWindow(rc);

		rc.left = rc.right + 4;
		rc.right = rc.left + 96;
		rc.top = rc.top + 0;
		rc.bottom = rc.bottom + 120;
		filter.getFilterColumnBox().MoveWindow(rc);

		rc.left = rc.right + 4;
		rc.right = rc.left + 96;
		rc.top = rc.top + 0;
		rc.bottom = rc.bottom + 120;
		filter.getFilterMethodBox().MoveWindow(rc);

	} else {
		rc.left = 0;
		rc.right = 0;
		filter.getFilterBox().MoveWindow(rc);
		filter.getFilterColumnBox().MoveWindow(rc);
		filter.getFilterMethodBox().MoveWindow(rc);
	}
}

void HubFrame::on(Disconnecting, const Client*) noexcept {
	SettingsManager::getInstance()->removeListener(this);
	FavoriteManager::getInstance()->removeListener(this);
	MessageManager::getInstance()->removeListener(this);

	client->removeListener(this);
	callAsync([this] {
		dcassert(frames.find(server) != frames.end());
		dcassert(frames[server] == this);
		frames.erase(server);
		clearTaskList();

		closed = true;

		PostMessage(WM_CLOSE);
	});
}

LRESULT HubFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	if(!closed) {
		if(shutdown || forceClose ||  WinUtil::MessageBoxConfirm(SettingsManager::CONFIRM_HUB_CLOSING, TSTRING(REALLY_CLOSE))) {
			ClientManager::getInstance()->putClient(client);
			return 0;
		}
	} else {
		SettingsManager::getInstance()->set(SettingsManager::GET_USER_INFO, showUsers);

		clearUserList();
		clearTaskList();
				
		string tmp, tmp2, tmp3;
		ctrlUsers.saveHeaderOrder(tmp, tmp2, tmp3);

		auto fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(Text::fromT(server));
		if(fhe) {
			CRect rc;
			if(!IsIconic()){
				//Get position of window
				GetWindowRect(&rc);
				
				//convert the position so it's relative to main window
				::ScreenToClient(GetParent(), &rc.TopLeft());
				::ScreenToClient(GetParent(), &rc.BottomRight());
				
				//save the position
				fhe->setBottom((uint16_t)(rc.bottom > 0 ? rc.bottom : 0));
				fhe->setTop((uint16_t)(rc.top > 0 ? rc.top : 0));
				fhe->setLeft((uint16_t)(rc.left > 0 ? rc.left : 0));
				fhe->setRight((uint16_t)(rc.right > 0 ? rc.right : 0));
			}

			fhe->setChatUserSplit(m_nProportionalPos);
			fhe->setUserListState(showUsers);
			fhe->setHeaderOrder(tmp);
			fhe->setHeaderWidths(tmp2);
			fhe->setHeaderVisible(tmp3);
			
			FavoriteManager::getInstance()->save();
		} else {
			SettingsManager::getInstance()->set(SettingsManager::HUBFRAME_ORDER, tmp);
			SettingsManager::getInstance()->set(SettingsManager::HUBFRAME_WIDTHS, tmp2);
			SettingsManager::getInstance()->set(SettingsManager::HUBFRAME_VISIBLE, tmp3);
		}
		bHandled = FALSE;
	}
	return 0;
}

void HubFrame::clearUserList() {
	for(auto& u: ctrlUsers)
		u.dec();

	ctrlUsers.DeleteAllItems();
}

void HubFrame::clearTaskList() {
	tasks.clear();
}

LRESULT HubFrame::onLButton(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	HWND focus = GetFocus();
	bHandled = false;
	if(focus == ctrlClient.m_hWnd) {
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

		const int i = ctrlClient.CharFromPos(pt);
		int line = ctrlClient.LineFromChar(i);
		const int c = i - ctrlClient.LineIndex(line);
		const int len = ctrlClient.LineLength(i);
		if(len < 3) {
			return 0;
		}

		TCHAR* buf = new TCHAR[len+1];
		ctrlClient.GetLine(line, buf, len+1);
		tstring x = tstring(buf, len);
		delete[] buf;

		string::size_type start = x.find_last_of(_T(" <\t\r\n"), c);

		if(start == string::npos)
			start = 0;
		else
			start++;
					

		string::size_type end = x.find_first_of(_T(" >\t"), start+1);

			if(end == string::npos) // get EOL as well
				end = x.length();
			else if(end == start + 1)
				return 0;

			// Nickname click, let's see if we can find one like it in the name list...
			tstring nick = x.substr(start, end - start);
			OnlineUserPtr ui = client->findUser(Text::fromT(nick));
			if(ui) {
				bHandled = true;
				if (wParam & MK_CONTROL) { // MK_CONTROL = 0x0008
					PrivateFrame::openWindow(HintedUser(ui->getUser(), client->getHubUrl()));
				} else if (wParam & MK_SHIFT) {
					try {
						QueueManager::getInstance()->addList(HintedUser(ui->getUser(), client->getHubUrl()), QueueItem::FLAG_CLIENT_VIEW);
					} catch(const Exception& e) {
						addStatus(Text::toT(e.getError()), LogMessage::SEV_ERROR, WinUtil::m_ChatTextSystem);
					}
				} else if(ui->getUser() != ClientManager::getInstance()->getMe()) {
					switch(SETTING(CHAT_DBLCLICK)) {
					case 0: {
						int items = ctrlUsers.GetItemCount();
						int pos = -1;
						ctrlUsers.SetRedraw(FALSE);
						for(int t = 0; t < items; ++t) {
							if(ctrlUsers.getItemData(t) == ui)
								pos = t;
							ctrlUsers.SetItemState(t, (t == pos) ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
						}
						ctrlUsers.SetRedraw(TRUE);
						ctrlUsers.EnsureVisible(pos, FALSE);
					    break;
					}    
					case 1: {
					     tstring sUser = ui->getText(OnlineUser::COLUMN_NICK);
					     int iSelBegin, iSelEnd;
					     ctrlMessage.GetSel(iSelBegin, iSelEnd);

					     if((iSelBegin == 0) && (iSelEnd == 0)) {
							sUser += _T(": ");
							if(ctrlMessage.GetWindowTextLength() == 0) {   
			                    ctrlMessage.SetWindowText(sUser.c_str());
			                    ctrlMessage.SetFocus();
			                    ctrlMessage.SetSel(ctrlMessage.GetWindowTextLength(), ctrlMessage.GetWindowTextLength());
							} else {
			                    ctrlMessage.ReplaceSel(sUser.c_str());
								ctrlMessage.SetFocus();
					        }
					     } else {
					          sUser += _T(" ");
					          ctrlMessage.ReplaceSel(sUser.c_str());
					          ctrlMessage.SetFocus();
					     }
					     break;
					}
					case 2:
						ui->pm();
					    break;
					case 3:
					    ui->getList();
					    break;
					case 4:
					    ui->matchQueue();
					    break;
					case 5:
					    ui->grant();
					    break;
					case 6:
					    ui->handleFav();
					    break;
				}
			}
		}
	}
	return 0;
}

void HubFrame::addLine(const tstring& aLine) {
	addLine(Identity(NULL, 0), aLine, WinUtil::m_ChatTextGeneral );
}

void HubFrame::addLine(const tstring& aLine, CHARFORMAT2& cf, bool bUseEmo/* = true*/) {
    addLine(Identity(NULL, 0), aLine, cf, bUseEmo);
}

void HubFrame::addLine(const Identity& i, const tstring& aLine, CHARFORMAT2& cf, bool bUseEmo/* = true*/) {
	bool notify = ctrlClient.AppendChat(i, Text::toT(client->get(HubSettings::Nick)), timeStamps ? Text::toT("[" + Util::getShortTimeString() + "] ") : Util::emptyStringT, aLine + _T('\n'), cf, bUseEmo);
	if(notify)
		setNotify();

	if (SETTING(BOLD_HUB)) {
		setDirty();
	}

}

LRESULT HubFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click 
	tabMenuShown = true;
	OMenu tabMenu, copyHubMenu;

	copyHubMenu.CreatePopupMenu();
	copyHubMenu.InsertSeparatorFirst(TSTRING(COPY));
	copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUBNAME, CTSTRING(NAME));
	copyHubMenu.AppendMenu(MF_STRING, IDC_COPY_HUBADDRESS, CTSTRING(HUB_ADDRESS));

	tabMenu.CreatePopupMenu();
	tabMenu.InsertSeparatorFirst(Text::toT(!client->getHubName().empty() ? (client->getHubName().size() > 50 ? (client->getHubName().substr(0, 50) + "...") : client->getHubName()) : client->getHubUrl()));	
	if(SETTING(LOG_MAIN_CHAT) || client->get(HubSettings::LogMainChat)) {
		tabMenu.AppendMenu(MF_STRING, IDC_OPEN_HUB_LOG, CTSTRING(OPEN_HUB_LOG));
		tabMenu.AppendMenu(MF_SEPARATOR);
		tabMenu.AppendMenu(MF_STRING, IDC_HISTORY, CTSTRING(VIEW_HISTORY));
		tabMenu.AppendMenu(MF_SEPARATOR);
	}

	tabMenu.AppendMenu(MF_STRING, ID_EDIT_CLEAR_ALL, CTSTRING(CLEAR_CHAT));
	if(client->get(HubSettings::ChatNotify)) 
		tabMenu.AppendMenu(MF_CHECKED, IDC_NOTIFY, CTSTRING(NOTIFY));
	else
		tabMenu.AppendMenu(MF_UNCHECKED, IDC_NOTIFY, CTSTRING(NOTIFY));

	tabMenu.AppendMenu(MF_SEPARATOR);
	tabMenu.AppendMenu(MF_STRING, IDC_ADD_AS_FAVORITE, CTSTRING(ADD_TO_FAVORITES));
	tabMenu.AppendMenu(MF_STRING, ID_FILE_RECONNECT, CTSTRING(MENU_RECONNECT));
	tabMenu.AppendMenu(MF_SEPARATOR);

	auto p = ShareManager::getInstance()->getShareProfile(client->getShareProfile());

	tabMenu.appendItem(CTSTRING_F(OPEN_HUB_FILELIST, Text::toT(p->getPlainName())), [this] { 
		handleOpenOwnList(); 
	}, p->getToken() == SP_HIDDEN ? OMenu::FLAG_DISABLED : 0);
	tabMenu.AppendMenu(MF_POPUP, (HMENU)copyHubMenu, CTSTRING(COPY));
	prepareMenu(tabMenu, ::UserCommand::CONTEXT_HUB, client->getHubUrl());
	tabMenu.AppendMenu(MF_SEPARATOR);
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE));
	
	if(!client->isConnected())
		tabMenu.EnableMenuItem((UINT_PTR)(HMENU)copyHubMenu, MF_GRAYED);
	else
		tabMenu.EnableMenuItem((UINT_PTR)(HMENU)copyHubMenu, MF_ENABLED);
	
	tabMenu.open(m_hWnd, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt);
	return TRUE;
}

void HubFrame::handleOpenOwnList(){
	DirectoryListingManager::getInstance()->openOwnList(client->getShareProfile());
}

LRESULT HubFrame::onSetNotify(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/){
	client->changeBoolHubSetting(HubSettings::ChatNotify);
	return 0;
}


LRESULT HubFrame::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	HDC hDC = (HDC)wParam;
	::SetBkColor(hDC, WinUtil::bgColor);
	::SetTextColor(hDC, WinUtil::textColor);
	return (LRESULT)WinUtil::bgBrush;
}
	
LRESULT HubFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	tabMenuShown = false;

	if(reinterpret_cast<HWND>(wParam) == ctrlUsers && showUsers && (ctrlUsers.GetSelectedCount() > 0)) {
		ctrlClient.setSelectedUser(Util::emptyStringT);
		if(pt.x == -1 && pt.y == -1) {
			WinUtil::getContextMenuPos(ctrlUsers, pt);
		}

		OMenu menu;
		menu.CreatePopupMenu();

		auto count = ctrlUsers.GetSelectedCount();
		bool isMe = false;

		if (count == 1) {
			auto sNick = Text::toT(((OnlineUser*) ctrlUsers.getItemData(ctrlUsers.GetNextItem(-1, LVNI_SELECTED)))->getIdentity().getNick());
			isMe = (sNick == Text::toT(client->getMyNick()));

			menu.InsertSeparatorFirst(sNick);

			if (SETTING(LOG_PRIVATE_CHAT)) {
				menu.AppendMenu(MF_STRING, IDC_OPEN_USER_LOG, CTSTRING(OPEN_USER_LOG));
				menu.AppendMenu(MF_STRING, IDC_USER_HISTORY, CTSTRING(VIEW_HISTORY));
				menu.AppendMenu(MF_SEPARATOR);
			}
		} else {
			menu.InsertSeparatorFirst(TSTRING_F(X_USERS, count));
		}

		if (!isMe) {
			menu.AppendMenu(MF_STRING, IDC_PUBLIC_MESSAGE, CTSTRING(SEND_PUBLIC_MESSAGE));
			appendUserItems(menu);

			if (count == 1) {
				const OnlineUserPtr ou = ctrlUsers.getItemData(ctrlUsers.GetNextItem(-1, LVNI_SELECTED));
				if (client->isOp() || !ou->getIdentity().isOp() || ou->getIdentity().isBot()) {
					if (!ou->getUser()->isIgnored()) {
						menu.AppendMenu(MF_STRING, IDC_IGNORE, CTSTRING(IGNORE_USER));
					} else {
						menu.AppendMenu(MF_STRING, IDC_UNIGNORE, CTSTRING(UNIGNORE_USER));
					}
				}
			}

			menu.AppendMenu(MF_SEPARATOR);
		}

		ctrlUsers.appendCopyMenu(menu);

		prepareMenu(menu, ::UserCommand::CONTEXT_USER, client->getHubUrl());
		menu.open(m_hWnd, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt);
	}
	bHandled = FALSE;
	return 0; 
}

void HubFrame::runUserCommand(::UserCommand& uc) {
	if(!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;

	auto ucParams = ucLineParams;

	client->getMyIdentity().getParams(ucParams, "my", true);
	client->getHubIdentity().getParams(ucParams, "hub", false);

	if(tabMenuShown) {
		client->sendUserCmd(uc, ucParams);
	} else {
		int sel = -1;
		while((sel = ctrlUsers.GetNextItem(sel, LVNI_SELECTED)) != -1) {
			const OnlineUserPtr u = ctrlUsers.getItemData(sel);
			if(u->getUser()->isOnline()) {
				auto tmp = ucParams;
				u->getIdentity().getParams(tmp, "user", true);
				client->sendUserCmd(uc, tmp);
			}
		}
	}
}

void HubFrame::onTab() {
	if(ctrlMessage.GetWindowTextLength() == 0) {
		handleTab(WinUtil::isShift());
		return;
	}
		
	HWND focus = GetFocus();
	if( (focus == ctrlMessage.m_hWnd) && !WinUtil::isShift() ) 
	{
		tstring text = WinUtil::getEditText(ctrlMessage);

		string::size_type textStart = text.find_last_of(_T(" \n\t"));

		if(complete.empty()) {
			if(textStart != string::npos) {
				complete = text.substr(textStart + 1);
			} else {
				complete = text;
			}
			if(complete.empty()) {
				// Still empty, no text entered...
				ctrlUsers.SetFocus();
				return;
			}
			int y = ctrlUsers.GetItemCount();

			for(int x = 0; x < y; ++x)
				ctrlUsers.SetItemState(x, 0, LVNI_FOCUSED | LVNI_SELECTED);
		}

		if(textStart == string::npos)
			textStart = 0;
		else
			textStart++;

		int start = ctrlUsers.GetNextItem(-1, LVNI_FOCUSED) + 1;
		int i = start;
		int j = ctrlUsers.GetItemCount();

		bool firstPass = i < j;
		if(!firstPass)
			i = 0;
		while(firstPass || (!firstPass && i < start)) {
			const OnlineUserPtr ui = ctrlUsers.getItemData(i);
			const tstring& nick = ui->getText(OnlineUser::COLUMN_NICK);
			bool found = (strnicmp(nick, complete, complete.length()) == 0);
			tstring::size_type x = 0;
			if(!found) {
				// Check if there's one or more [ISP] tags to ignore...
				tstring::size_type y = 0;
				while(nick[y] == _T('[')) {
					x = nick.find(_T(']'), y);
					if(x != string::npos) {
						if(strnicmp(nick.c_str() + x + 1, complete.c_str(), complete.length()) == 0) {
							found = true;
							break;
						}
					} else {
						break;
					}
					y = x + 1; // assuming that nick[y] == '\0' is legal
				}
			}
			if(found) {
				if((start - 1) != -1) {
					ctrlUsers.SetItemState(start - 1, 0, LVNI_SELECTED | LVNI_FOCUSED);
				}
				ctrlUsers.SetItemState(i, LVNI_FOCUSED | LVNI_SELECTED, LVNI_FOCUSED | LVNI_SELECTED);
				ctrlUsers.EnsureVisible(i, FALSE);
				ctrlMessage.SetSel(textStart, ctrlMessage.GetWindowTextLength(), TRUE);
				ctrlMessage.ReplaceSel(nick.c_str());
				return;
			}
			i++;
			if(i == j) {
				firstPass = false;
				i = 0;
			}
		}
	}
}

LRESULT HubFrame::onFileReconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	client->reconnect();
	return 0;
}

LRESULT HubFrame::onShowUsers(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
	bHandled = FALSE;
	if(wParam == BST_CHECKED) {
		showUsers = true;
		client->refreshUserList(true);
	} else {
		showUsers = false;
		clearUserList();
	}

	SettingsManager::getInstance()->set(SettingsManager::GET_USER_INFO, showUsers);

	UpdateLayout(FALSE);
	return 0;
}

LRESULT HubFrame::onFollow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	client->doRedirect();
	return 0;
}

void HubFrame::on(Redirected, const string&, const ClientPtr& aNewClient) noexcept {
	callAsync([=] {
		client->removeListener(this);
		clearUserList();
		clearTaskList();
		frames.erase(server);

		server = Text::toT(aNewClient->getHubUrl());
		frames[server] = this;

		client = aNewClient;
		aNewClient->addListener(this);
		ctrlClient.setClient(aNewClient);

		aNewClient->connect();
	});
}

LRESULT HubFrame::onEnterUsers(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/) {
	int item = ctrlUsers.GetNextItem(-1, LVNI_FOCUSED);
	if(item != -1) {
		try {
			QueueManager::getInstance()->addList(HintedUser((ctrlUsers.getItemData(item))->getUser(), client->getHubUrl()), QueueItem::FLAG_CLIENT_VIEW);
		} catch(const Exception& e) {
			addStatus(Text::toT(e.getError()), LogMessage::SEV_ERROR);
		}
	}
	return 0;
}

LRESULT HubFrame::onGetToolTip(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/) {
	NMTTDISPINFO* nm = (NMTTDISPINFO*)pnmh;
	LPNMTTDISPINFO pDispInfo = (LPNMTTDISPINFO)pnmh;
	pDispInfo->szText[0] = 0;

	if (idCtrl == 1 + POPUP_UID && !cipherPopupTxt.empty()) {
		nm->lpszText = const_cast<TCHAR*>(cipherPopupTxt.c_str());
		return 0;
	}

	if (idCtrl == 0 + POPUP_UID) {
		lastLines.clear();
		for (auto& i : lastLinesList) {
			lastLines += i;
			lastLines += _T("\r\n");
		}

		if (lastLines.size() > 2) {
			lastLines.erase(lastLines.size() - 2);
		}

		nm->lpszText = const_cast<TCHAR*>(lastLines.c_str());
	}
	return 0;
}

void HubFrame::addStatus(const tstring& aLine, uint8_t sev, CHARFORMAT2& cf, bool inChat /* = true */) {
	tstring line = _T("[") + Text::toT(Util::getShortTimeString()) + _T("] ") + aLine;
	TCHAR* sLine = (TCHAR*)line.c_str();

   	if(line.size() > 512) {
		sLine[512] = NULL;
	}

	if(SETTING(HUB_BOLD_TABS))
		setDirty();


	ctrlStatus.SetText(0, sLine, SBT_NOTABPARSING);
	ctrlStatus.SetIcon(0, ResourceLoader::getSeverityIcon(sev));
	while(lastLinesList.size() + 1 > MAX_CLIENT_LINES)
		lastLinesList.pop_front();
	lastLinesList.push_back(sLine);

	if (SETTING(BOLD_HUB)) {
		setDirty();
	}
	
	if(SETTING(STATUS_IN_CHAT) && inChat) {
		addLine(_T("*** ") + aLine, cf, SETTING(HUB_BOLD_TABS));
	}
}

void HubFrame::resortUsers() {
	for(auto f: frames | map_values)
		f->resortForFavsFirst(true);
}

void HubFrame::closeDisconnected() {
	for(auto f: frames | map_values) {
		if (!f->client->isConnected()) {
			f->forceClose = true;
			f->PostMessage(WM_CLOSE);
		}
	}
}

void HubFrame::updateFonts() {
	for(auto f: frames | map_values) {
		f->setFonts();
		f->UpdateLayout();
	}
}

void HubFrame::reconnectDisconnected() {
	for(auto f: frames | map_values) {
		if (!f->client->isConnected()) {
			f->client->disconnect(false); 
			f->clearUserList();
			f->client->connect(); 
		}
	}
}

void HubFrame::on(FavoriteManagerListener::UserAdded, const FavoriteUser& /*aUser*/) noexcept{
	callAsync([=] { updateUsers = true; });
	resortForFavsFirst();
}
void HubFrame::on(FavoriteManagerListener::UserRemoved, const FavoriteUser& /*aUser*/) noexcept {
	callAsync([=] { updateUsers = true; });
	resortForFavsFirst();
}

void HubFrame::resortForFavsFirst(bool justDoIt /* = false */) {
	if(justDoIt || SETTING(SORT_FAVUSERS_FIRST)) {
		resort = true;
		callAsync([this] { execTasks(); });
	}
}

LRESULT HubFrame::onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/) {
	//updateUsers = true;
	return 0;
}

LRESULT HubFrame::onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if(updateUsers) {
		updateUsers = false;
		callAsync([this] { execTasks(); });
	}

	if(statusDirty) {
		statusDirty = false;
		updateStatusBar();
	}
	return 0;
}

void HubFrame::updateStatusBar() { 
	size_t AllUsers = client->getUserCount();
	size_t ShownUsers = ctrlUsers.GetItemCount();

	tstring text[3];

	if(AllUsers != ShownUsers) {
		text[0] = Util::toStringW(ShownUsers) + _T("/") + Util::toStringW(AllUsers) + _T(" ") + Text::toLower(TSTRING(USERS));
	} else {
		text[0] = TSTRING_F(X_USERS, AllUsers);
	}
	int64_t available = client->getTotalShare();
	text[1] = Util::formatBytesW(available);

	if(AllUsers > 0)
		text[2] = Util::formatBytesW(available / AllUsers) + _T("/") + Text::toLower(TSTRING(USER));

	bool update = false;
	for(int i = 0; i < 3; i++) {
		int size = WinUtil::getTextWidth(text[i], ctrlStatus.m_hWnd);
		if(size != statusSizes[i + 1]) {
			statusSizes[i + 1] = size;
			update = true;
		}
		ctrlStatus.SetText(i + 2, text[i].c_str());
	}

	if(update)
		UpdateLayout();
}

void HubFrame::on(Connecting, const Client*) noexcept { 
	callAsync([=] {
		if(SETTING(SEARCH_PASSIVE) && client->isActive()) {
			addLine(TSTRING(ANTI_PASSIVE_SEARCH), WinUtil::m_ChatTextSystem);
		}

		setWindowTitle(client->getHubUrl());
	});
}
void HubFrame::on(Connected, const Client*) noexcept { 
	callAsync([=] { onConnected(); });
}

void HubFrame::on(UserConnected, const Client* c, const OnlineUserPtr& user) noexcept {
	on(UserUpdated(), c, user);
}

void HubFrame::on(UserUpdated, const Client*, const OnlineUserPtr& user) noexcept {
	//If its my identity, check if we need to update the tab icon
	if (user->getUser() == ClientManager::getInstance()->getMe()) {
		onUpdateTabIcons(); 
	}
	
	auto task = new UserTask(user);
	tasks.add(UPDATE_USER_JOIN, unique_ptr<Task>(task));
	updateUsers = true;
}
void HubFrame::on(UsersUpdated, const Client*, const OnlineUserList& aList) noexcept {
	for(auto& i: aList) {
		//If its my identity, check if we need to update the tab icon
		if (i->getUser() == ClientManager::getInstance()->getMe()) {
			onUpdateTabIcons();
		}

		auto task = new UserTask(i);
		tasks.add(UPDATE_USER, unique_ptr<Task>(task));
	}
	updateUsers = true;
}

void HubFrame::on(ClientListener::UserRemoved, const Client*, const OnlineUserPtr& user) noexcept {
	auto task = new UserTask(user);
	tasks.add(REMOVE_USER, unique_ptr<Task>(task));
	updateUsers = true;
}

void HubFrame::on(Redirect, const Client*, const string& line) noexcept { 
	callAsync([=] { 
		addStatus(Text::toT(STRING(PRESS_FOLLOW) + " " + line), LogMessage::SEV_INFO, WinUtil::m_ChatTextServer); 
	});
}

void HubFrame::on(Failed, const string&, const string& line) noexcept {
	callAsync([=] {
		onDisconnected(line);
	});
}
void HubFrame::on(GetPassword, const Client*) noexcept { 
	callAsync([=] {
		onPassword();
	});
}
void HubFrame::on(HubUpdated, const Client*) noexcept {
	string hubName;
	if(client->isTrusted()) {
		hubName = "[S] ";
	} else if(client->isSecure()) {
		hubName = "[U] ";
	}
	
	hubName += client->getHubName();
	if(!client->getHubDescription().empty()) {
		hubName += " - " + client->getHubDescription();
		cachedHubname = client->getHubDescription();
	}
	if(wentoffline && !cachedHubname.empty())
		hubName += " - " + cachedHubname;

	hubName += " (" + client->getHubUrl() + ")";

#ifdef _DEBUG
	string version = client->getHubIdentity().get("VE");
	if(!version.empty()) {
		hubName += " - " + version;
	}
#endif

	callAsync([=] {
		setWindowTitle(hubName);
	});
}
void HubFrame::on(ChatMessage, const Client*, const ChatMessagePtr& message) noexcept {
	callAsync([=] { onChatMessage(message); });
}	

void HubFrame::on(StatusMessage, const Client*, const LogMessagePtr& aMessage, int statusFlags) noexcept {
	callAsync([=] { 
		if(SETTING(BOLD_HUB_TABS_ON_KICK) && (statusFlags & ClientListener::FLAG_IS_SPAM)){
			setDirty();
		}

		onStatusMessage(aMessage, statusFlags & ClientListener::FLAG_IS_SPAM);
	});

}
void HubFrame::onStatusMessage(const LogMessagePtr& aMessage, bool isSpam) noexcept {
	addStatus(Text::toT(Text::toDOS(aMessage->getText())), aMessage->getSeverity(), WinUtil::m_ChatTextServer, !SETTING(FILTER_MESSAGES) || !isSpam);
}


void HubFrame::on(MessagesRead) noexcept {

}

void HubFrame::on(NickTaken, const Client*) noexcept {
	callAsync([=] { addStatus(TSTRING(NICK_TAKEN), LogMessage::SEV_ERROR, WinUtil::m_ChatTextServer); });
}
void HubFrame::on(SearchFlood, const Client*, const string& line) noexcept {
	callAsync([=] { addStatus(TSTRING(SEARCH_SPAM_FROM) + _T(" ") + Text::toT(line), LogMessage::SEV_INFO, WinUtil::m_ChatTextServer); });
}

void HubFrame::on(HubTopic, const Client*, const string& line) noexcept {
	callAsync([=] { addStatus(TSTRING(HUB_TOPIC) + _T("\t") + Text::toT(line), LogMessage::SEV_INFO, WinUtil::m_ChatTextServer); });
}
void HubFrame::on(AddLine, const Client*, const string& line) noexcept {
	callAsync([=] { addStatus(Text::toT(line), LogMessage::SEV_INFO, WinUtil::m_ChatTextServer); });
}

void HubFrame::on(SetActive, const Client*) noexcept {
	callAsync([=] { 
		if (::IsIconic(m_hWnd))
			::ShowWindow(m_hWnd, SW_RESTORE);
		MDIActivate(m_hWnd);
	});
}

void HubFrame::on(MessageManagerListener::IgnoreAdded, const UserPtr&) noexcept{
	callAsync([=] { updateUsers = true; });
}

void HubFrame::on(MessageManagerListener::IgnoreRemoved, const UserPtr&) noexcept{
	callAsync([=] { updateUsers = true; });
}

void HubFrame::openLinksInTopic() {
	StringList urls;
	
	boost::regex linkReg(AirUtil::getLinkUrl());
	AirUtil::getRegexMatches(client->getHubDescription(), urls, linkReg);

	for(auto& url: urls) {
		Util::sanitizeUrl(url);
		WinUtil::openLink(Text::toT(url));
	}
}

void HubFrame::updateUserList(OnlineUserPtr aUser) {
	
	//single update?
	//avoid refreshing the whole list and just update the current item
	//instead
	auto filterInfoF = [this, &aUser](int column) { return Text::fromT(aUser->getText(column)); };
	auto filterNumericF = [&](int column) -> double {
		switch (column) {
		case OnlineUser::COLUMN_EXACT_SHARED:
		case OnlineUser::COLUMN_SHARED: return aUser->getIdentity().getBytesShared();
		case OnlineUser::COLUMN_SLOTS: return aUser->getIdentity().getSlots();
		case OnlineUser::COLUMN_DLSPEED: return aUser->getIdentity().getAdcConnectionSpeed(true);
		case OnlineUser::COLUMN_ULSPEED: return aUser->getUser()->isNMDC() ? 
			Util::toFloat(aUser->getIdentity().getConnectionString()) : aUser->getIdentity().getAdcConnectionSpeed(false);
		case OnlineUser::COLUMN_FILES: return Util::toFloat(aUser->getIdentity().getSharedFiles());
		case OnlineUser::COLUMN_HUBS: return aUser->getIdentity().getTotalHubCount();
		default: dcassert(0); return 0;
		}
	};

	auto filterPrep = filter.prepare(filterInfoF, filterNumericF);

	if(aUser) {
		if(aUser->isHidden()) {
			return;
		}


		if (filter.empty() || filter.match(filterPrep)) {
			if (ctrlUsers.findItem(aUser.get()) == -1) {
				aUser->inc();
				ctrlUsers.insertItem(aUser.get(), aUser->getImageIndex());
			}
		} else {
			int i = ctrlUsers.findItem(aUser.get());
			if (i != -1) {
				ctrlUsers.DeleteItem(i);
				aUser->dec();
			}
		}
	} else {
		ctrlUsers.SetRedraw(FALSE);
		clearUserList();

		OnlineUserList l;
		client->getUserList(l, false);

		if(filter.empty()) {
			for(const auto& ui: l) {
				ui->inc();
				ctrlUsers.insertItem(ui.get(), ui->getImageIndex());
			}
		} else {
			auto i = l.begin();
			for(; i != l.end(); ++i){
				// really hacky because of the filter
				aUser = *i;
				if (filter.empty() || filter.match(filterPrep)) {
					aUser->inc();
					ctrlUsers.insertItem(aUser.get(), aUser->getImageIndex());
				}
			}
		}
		ctrlUsers.SetRedraw(TRUE);
	}

	statusDirty = true;
}
void HubFrame::handleTab(bool reverse) {
	HWND focus = GetFocus();

	if(reverse) {
		if(focus == filter.getFilterMethodBox().m_hWnd) {
			filter.getFilterColumnBox().SetFocus();
		} else if(focus == filter.getFilterColumnBox().m_hWnd) {
			filter.getFilterBox().SetFocus();
		} else if(focus == filter.getFilterBox().m_hWnd) {
			ctrlMessage.SetFocus();
		} else if(focus == ctrlMessage.m_hWnd) {
			ctrlUsers.SetFocus();
		} else if(focus == ctrlUsers.m_hWnd) {
			ctrlClient.SetFocus();
		} else if(focus == ctrlClient.m_hWnd) {
			filter.getFilterMethodBox().SetFocus();
		}
	} else {
		if(focus == ctrlClient.m_hWnd) {
			ctrlUsers.SetFocus();
		} else if(focus == ctrlUsers.m_hWnd) {
			ctrlMessage.SetFocus();
		} else if(focus == ctrlMessage.m_hWnd) {
			filter.getFilterBox().SetFocus();
		} else if(focus == filter.getFilterBox().m_hWnd) {
			filter.getFilterColumnBox().SetFocus();
		} else if(focus == filter.getFilterColumnBox().m_hWnd) {
			filter.getFilterMethodBox().SetFocus();
		} else if(focus == filter.getFilterMethodBox().m_hWnd) {
			ctrlClient.SetFocus();
		}
	}
}

LRESULT HubFrame::onSelectUser(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if(ctrlClient.getSelectedUser().empty()) {
		// No nick selected
		return 0;
	}

	int pos = ctrlUsers.findItem(ctrlClient.getSelectedUser());
	if ( pos == -1 ) {
		// User not found is list
		return 0;
	}

	int items = ctrlUsers.GetItemCount();
	ctrlUsers.SetRedraw(FALSE);
	for(int i = 0; i < items; ++i) {
		ctrlUsers.SetItemState(i, (i == pos) ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
	}
	ctrlUsers.SetRedraw(TRUE);
	ctrlUsers.EnsureVisible(pos, FALSE);

	return 0;
}

LRESULT HubFrame::onPublicMessage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int i = -1;
	tstring sUsers = Util::emptyStringT;

	if(!client->isConnected())
		return 0;

	if(ctrlClient.getSelectedUser().empty()) {
		while( (i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1) {
			if (!sUsers.empty())
				sUsers += _T(", ");
			sUsers += Text::toT(((OnlineUser*)ctrlUsers.getItemData(i))->getIdentity().getNick());
		}
	} else {
		sUsers = ctrlClient.getSelectedUser();
	}

	int iSelBegin, iSelEnd;
	ctrlMessage.GetSel( iSelBegin, iSelEnd );

	if ( ( iSelBegin == 0 ) && ( iSelEnd == 0 ) ) {
		sUsers += _T(": ");
		if (ctrlMessage.GetWindowTextLength() == 0) {	
			ctrlMessage.SetWindowText(sUsers.c_str());
			ctrlMessage.SetFocus();
			ctrlMessage.SetSel( ctrlMessage.GetWindowTextLength(), ctrlMessage.GetWindowTextLength() );
		} else {
			ctrlMessage.ReplaceSel( sUsers.c_str() );
			ctrlMessage.SetFocus();
		}
	} else {
		sUsers += _T(" ");
		ctrlMessage.ReplaceSel( sUsers.c_str() );
		ctrlMessage.SetFocus();
	}
	return 0;
}

LRESULT HubFrame::onOpenUserLog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {	
	ParamMap params;
	OnlineUserPtr ui = nullptr;

	int i = -1;
	if((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1) {
		ui = ctrlUsers.getItemData(i);
	}

	if(!ui) 
		return 0;

	string file = ui->getLogPath();
	if(Util::fileExists(file)) {
		WinUtil::viewLog(file, wID == IDC_USER_HISTORY);
	} else {
		WinUtil::showMessageBox(TSTRING(NO_LOG_FOR_USER));
	}
	return 0;
}

string HubFrame::getLogPath(bool status) const {
	ParamMap params;
	params["hubNI"] = [this] { return client->getHubName(); };
	params["hubURL"] = [this] { return client->getHubUrl(); };
	params["myNI"] = [this] { return client->getMyNick(); };
	return LogManager::getInstance()->getPath(status ? LogManager::STATUS : LogManager::CHAT, params);
}

LRESULT HubFrame::onOpenHubLog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	string filename = getLogPath(false);
	if(Util::fileExists(filename)){
		WinUtil::viewLog(filename, wID == IDC_HISTORY);
	} else {
		WinUtil::showMessageBox(TSTRING(NO_LOG_FOR_HUB));	  
	}
	return 0;
}

LRESULT HubFrame::onStyleChange(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled) {
	bHandled = FALSE;
	if((wParam & MK_LBUTTON) && ::GetCapture() == m_hWnd) {
		UpdateLayout(FALSE);
	}
	return 0;
}

LRESULT HubFrame::onStyleChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	bHandled = FALSE;
	UpdateLayout(FALSE);
	return 0;
}

void HubFrame::on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept {
	bool needRedraw = false;
	ctrlUsers.SetImageList(ResourceLoader::getUserImages(), LVSIL_SMALL);
	//ctrlUsers.Invalidate();
	if(ctrlUsers.GetBkColor() != WinUtil::bgColor) {
		needRedraw = true;
		ctrlClient.SetBackgroundColor(WinUtil::bgColor);
		ctrlUsers.SetBkColor(WinUtil::bgColor);
		ctrlUsers.SetTextBkColor(WinUtil::bgColor);
		ctrlUsers.setFlickerFree(WinUtil::bgBrush);
	}
	if(ctrlUsers.GetTextColor() != WinUtil::textColor) {
		needRedraw = true;
		ctrlUsers.SetTextColor(WinUtil::textColor);
	}
	if(ctrlUsers.GetFont() != WinUtil::listViewFont){
		ctrlUsers.SetFont(WinUtil::listViewFont);
		needRedraw = true;
	}

	if(needRedraw)
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

LRESULT HubFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/) {
	LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)pnmh;

	switch(cd->nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;

	case CDDS_ITEMPREPAINT: {
			OnlineUser* ui = (OnlineUser*)cd->nmcd.lItemlParam;
			// tstring user = ui->getText(OnlineUser::COLUMN_NICK); 
			if (ui->getUser()->isFavorite()) {
				cd->clrText = SETTING(FAVORITE_COLOR);
			} else if (UploadManager::getInstance()->hasReservedSlot(ui->getUser())) {
				cd->clrText = SETTING(RESERVED_SLOT_COLOR);
			} else if (ui->getUser()->isIgnored()) {
				cd->clrText = SETTING(IGNORED_COLOR);
			} else if(ui->getIdentity().isOp()) {
				cd->clrText = SETTING(OP_COLOR);
			} else if(!ui->getIdentity().isTcpActive(client)) {
				cd->clrText = SETTING(PASIVE_COLOR);
			} else {
				cd->clrText = SETTING(NORMAL_COLOUR);
			}
			if( SETTING(USE_HIGHLIGHT) ) {
				
				ColorList *cList = HighlightManager::getInstance()->getList();
				for(ColorIter i = cList->begin(); i != cList->end(); ++i) {
					ColorSettings* cs = &(*i);
					string str;
					if(cs->getContext() == HighlightManager::CONTEXT_NICKLIST) {
						tstring match = ui->getText(cs->getMatchColumn());
						if(match.empty()) continue;
						if(cs->usingRegexp()) {
							try {
								//have to have $Re:
								if(boost::regex_search(match.begin(), match.end(), cs->regexp)){
									if(cs->getHasFgColor()) cd->clrText = cs->getFgColor();
									break;
								}
							}catch(...) {}
						} else {
							if (Wildcard::patternMatch(Text::utf8ToAcp(Text::fromT(match)), Text::utf8ToAcp(Text::fromT(cs->getMatch())), '|')){
								if(cs->getHasFgColor()) cd->clrText = cs->getFgColor();
								break;
								}
							}
						}
					}
				}
			
			return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
		}

	case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
		if(SETTING(GET_USER_COUNTRY) && (ctrlUsers.findColumn(cd->iSubItem) == OnlineUser::COLUMN_IP4 || ctrlUsers.findColumn(cd->iSubItem) == OnlineUser::COLUMN_IP6)) {
			CRect rc;
			OnlineUser* ou = (OnlineUser*)cd->nmcd.lItemlParam;
			ctrlUsers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);

			SetTextColor(cd->nmcd.hdc, cd->clrText);
			DrawThemeBackground(GetWindowTheme(ctrlUsers.m_hWnd), cd->nmcd.hdc, LVP_LISTITEM, 3, &rc, &rc );

			TCHAR buf[256];
			ctrlUsers.GetItemText((int)cd->nmcd.dwItemSpec, cd->iSubItem, buf, 255);
			buf[255] = 0;
			if(_tcslen(buf) > 0) {
				rc.left += 2;
				LONG top = rc.top + (rc.Height() - 15)/2;
				if((top - rc.top) < 2)
					top = rc.top + 1;

				POINT p = { rc.left, top };

				string ip = ou->getIdentity().getIp();
				uint8_t flagIndex = 0;
				if (!ip.empty()) {
					// Only attempt to grab a country mapping if we actually have an IP address
					string tmpCountry = GeoManager::getInstance()->getCountry(ip);
					if(!tmpCountry.empty()) {
						flagIndex = Localization::getFlagIndexByCode(tmpCountry.c_str());
					}
				}

				ResourceLoader::flagImages.Draw(cd->nmcd.hdc, flagIndex, p, LVSIL_SMALL);
				top = rc.top + (rc.Height() - WinUtil::getTextHeight(cd->nmcd.hdc) - 1)/2;
				::ExtTextOut(cd->nmcd.hdc, rc.left + 30, top + 1, ETO_CLIPPED, rc, buf, _tcslen(buf), NULL);
				return CDRF_SKIPDEFAULT;
			}
		}		
	}

	default:
		return CDRF_DODEFAULT;
	}
}

LRESULT HubFrame::onKeyDownUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/) {
	NMLVKEYDOWN* l = (NMLVKEYDOWN*)pnmh;
	if(l->wVKey == VK_TAB) {
		onTab();
	} else if(WinUtil::isCtrl()) {
		int i = -1;
		switch(l->wVKey) {
			case 'M':
				while( (i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1) {
					OnlineUserPtr ou = ctrlUsers.getItemData(i);
					if(ou->getUser() != ClientManager::getInstance()->getMe())
					{
						ou->pm();
					}
				}				
				break;
			// TODO: add others
		}
	}
	return 0;
}

LRESULT HubFrame::onEditClearAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	ctrlClient.SetWindowText(Util::emptyStringT.c_str());
	return 0;
}

void HubFrame::setFonts() {
	/* 
	Pretty brave attemp to switch font on an open window, hope it dont cause any trouble.
	Reset the fonts. This will reset the charformats in the window too :( 
		they will apply again with new text..
		*/
	ctrlClient.SetRedraw(FALSE);
	ctrlClient.SetSelAll();

	ctrlClient.SetFont(WinUtil::font, FALSE);
	ctrlMessage.SetFont(WinUtil::font, FALSE);
	
	ctrlClient.SetSelectionCharFormat(WinUtil::m_ChatTextLog);
	ctrlClient.SetSelNone();
	ctrlClient.SetRedraw(TRUE);

	addStatus(TSTRING(NEW_TEXT_STYLE_APPLIED), LogMessage::SEV_INFO, WinUtil::m_ChatTextSystem);
}

LRESULT HubFrame::onIgnore(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/) {
	int i=-1;
	while( (i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1) {
		MessageManager::getInstance()->storeIgnore(((OnlineUser*)ctrlUsers.getItemData(i))->getUser());
	}
	return 0;
}

LRESULT HubFrame::onUnignore(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/) {
	int i=-1;
	while( (i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1) {
		MessageManager::getInstance()->removeIgnore(((OnlineUser*)ctrlUsers.getItemData(i))->getUser());
	}
	return 0;
}