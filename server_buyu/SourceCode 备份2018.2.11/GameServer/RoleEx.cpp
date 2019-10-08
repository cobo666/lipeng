#include "StdAfx.h"
#include "RoleEx.h"
#include "Role.h"
#include "RoleManager.h"
#include "FishServer.h"
#include "..\CommonFile\DBLogManager.h"
#include "FishLogic\NetCmd.h"
extern void SendLogDB(NetCmd* pCmd);
CRoleEx::CRoleEx()
{
	m_IsNeedSave = false;
	m_IsChangeClientIP = false;
	m_LogonTimeByDay = 0;
	m_LogobByGameServer = false;
	m_ChannelID = 0;//����ID Ĭ��Ϊ0

	m_IsRobot = false;//�Ƿ�Ϊ������
	m_IsAfk = false;//�Ƿ�����
	m_IsExit = false;
	m_IsOnline = false;//�Ƿ�����
}
CRoleEx::~CRoleEx()
{
	//WORD OnLineMin = static_cast<WORD>(GetRoleOnlineSec() / 60);//��ȡ������ߵķ���
	//if (m_RoleInfo.OnlineMin != OnLineMin)
	//{
	//	m_RoleInfo.OnlineMin = OnLineMin;
	//	m_IsNeedSave = true;
	//}
	//SaveRoleExInfo();//������ߵ�ʱ�� �����Ҫ�������� �ͱ����
	if (!m_ChannelUserInfo.empty())
	{
		std::vector<TCHAR*>::iterator Iter = m_ChannelUserInfo.begin();
		for (; Iter != m_ChannelUserInfo.end(); ++Iter)
		{
			free(*Iter);
		}
		m_ChannelUserInfo.clear();
	}

	m_IsAfk = false;
	m_IsExit = false;
	m_IsOnline = false;
}
//��ҵ�¼��ʼ�������Ϣ 
bool CRoleEx::OnInit(tagRoleInfo* pUserInfo, tagRoleServerInfo* pRoleServerInfo, RoleManager* pManager, DWORD dwSocketID, time_t pTime, bool LogobByGameServer, bool IsRobot)//��ҵ�½�ɹ���ʱ�� dwSocketID ��Ӧ��Gate��ID
{
	if (!pUserInfo || !pRoleServerInfo)
	{
		ASSERT(false);
		return false;
	}
	ServerClientData * pClient = g_FishServer.GetUserClientDataByIndex(dwSocketID);
	if (dwSocketID != 0 && !pClient)
	{
		ASSERT(false);
		return false;
	}
	m_IsRobot = IsRobot;
	m_dwGameSocketID = dwSocketID;
	m_RoleInfo = *pUserInfo;
	m_RoleServerInfo = *pRoleServerInfo;

	{
		m_RoleInfo.benefitCount = m_RoleServerInfo.RoleProtectSum;
		m_RoleInfo.benefitTime = (DWORD)m_RoleServerInfo.RoleProtectLogTime;
	}

	m_RoleInfo.AchievementPointIndex = g_FishServer.GetAchievementIndex(m_RoleInfo.dwUserID);//������ҵ�����

	if (pClient && m_RoleInfo.ClientIP != pClient->IP)
	{
		m_RoleInfo.ClientIP = pClient->IP;//������߳ɹ���ȡ��ҵ�IP��ַ

		//��ҵ�IP��ַ�����仯�� ������Ҫ���д��� ����л�IP��½��
		m_IsNeedSave = true;
		m_IsChangeClientIP = true;
	}
	else
	{
		m_IsChangeClientIP = false;
	}

	g_FishServer.GetAddressByIP(m_RoleInfo.ClientIP, m_RoleInfo.IPAddress, CountArray(m_RoleInfo.IPAddress));//������ҵ�IPλ��

	m_RoleManager = pManager;
	m_LastOnLineTime = pTime;
	m_LogonTime = time(NULL);
	m_LogonTimeByDay = m_LogonTime;
	//����ǲ���ͬһ���¼
	if (!IsOnceDayOnline())
	{
		//�Ƿ�Ϊͬһ���½ ÿ���������
		m_RoleInfo.dwProduction = 0;
		m_RoleInfo.dwGameTime = 0;
		m_RoleInfo.OnlineMin = 0;
		ResetPerDay();//

		ChangeRoleSendGiffSum(m_RoleInfo.SendGiffSum * -1);//��������
		ChangeRoleCashSum(m_RoleInfo.CashSum * -1);//�һ�����
		ChangeRoleAcceptGiffSum(m_RoleInfo.AcceptGiffSum * -1);//jiesh��������
		ClearRoleTaskStates();//�޸�����
		ChangeRoleOnlineRewardStates(0);//�����������
		m_IsNeedSave = true;
		if (!IsOnceMonthOnline())//�ǲ��ǵ��µ�һ�ε�¼
		{
			ChangeRoleCheckData(0);//���ɱ���  ǩ��������
		}

	}
	m_LogobByGameServer = LogobByGameServer;

	ChannelUserInfo* pInfo = g_FishServer.GetChannelUserInfo(GetUserID());//�������
	if (pInfo)
	{
		//����һ����ҵ���������
		DWORD PageSize = sizeof(DBR_Cmd_SaveChannelInfo)+sizeof(BYTE)*(pInfo->Sum - 1);
		DWORD InfoSize = sizeof(ChannelUserInfo)+sizeof(BYTE)*(pInfo->Sum - 1);
		DBR_Cmd_SaveChannelInfo* msgDB = (DBR_Cmd_SaveChannelInfo*)malloc(PageSize);
		if (!msgDB)
		{
			ASSERT(false);
			return false;
		}
		msgDB->SetCmdType(DBR_SaveChannelInfo);
		msgDB->SetCmdSize(static_cast<WORD>(PageSize));
		msgDB->dwUserID = GetUserID();
		memcpy_s(&msgDB->pInfo, InfoSize, pInfo, InfoSize);
		g_FishServer.SendNetCmdToSaveDB(msgDB);
		free(msgDB);

		GetStringArrayVecByData(m_ChannelUserInfo, pInfo);
		if (m_ChannelUserInfo.size() != pInfo->HandleSum)
		{
			ASSERT(false);
			m_ChannelUserInfo.clear();
			m_ChannelID = 0;
		}
		//��ȡ��������
		m_ChannelID = GetCrc32(m_ChannelUserInfo[1]);

		g_FishServer.OnDelChannelInfo(GetUserID());//����ҵ���������ȡ��
	}
	else
	{
		m_ChannelUserInfo.clear();
		m_ChannelID = 0;
	}

	string MacAddress = g_FishServer.GetUserMacAddress(GetUserID());
	string IPAddress = g_FishServer.GetUserIpAddress(GetUserID());
	g_DBLogManager.LogRoleOnlineInfo(m_RoleInfo.dwUserID, true, MacAddress, IPAddress, m_RoleInfo.dwGlobeNum, m_RoleInfo.dwCurrencyNum, m_RoleInfo.dwMedalNum, SendLogDB);
	
	LogInfoToFile("CollideBuYu.txt", "OnInit��ҵ�½�ɹ���ʱ��:::::���::::%d ::::::::���:::%d:::", m_RoleInfo.dwUserID, m_RoleInfo.dwGlobeNum);


	return m_RelationManager.OnInit(this, pManager) && m_ItemManager.OnInit(this, pManager) && m_MailManager.OnInit(this, pManager) /*&& m_RoleFtpFaceManager.OnInit(this)*/
		&& m_RoleCheck.OnInit(this) && m_RoleTask.OnInit(this) && m_RoleAchievement.OnInit(this) && m_RoleMonth.OnInit(this) && m_RoleTitleManager.OnInit(this) &&
		m_RoleIDEntity.OnInit(this) && m_RoleActionManager.OnInit(this) && m_RoleGiffManager.OnInit(this) && m_RoleFtpManager.OnInit(this) && m_RoleGameData.OnInit(this) && m_RoleRank.OnInit(this)
		&& m_RoleProtect.OnInit(this) && m_RoleVip.OnInit(this) && m_RoleMonthCard.OnInit(this) && m_RoleRate.OnInit(this) && m_RoleCharManger.OnInit(this) && m_RoleRelationRequest.OnInit(this);
}
bool CRoleEx::IsLoadFinish()
{
	if (
		m_RelationManager.IsLoadDB() &&
		m_ItemManager.IsLoadDB() &&
		m_MailManager.IsLoadDB() &&
		m_RoleTask.IsLoadDB() &&
		m_RoleAchievement.IsLoadDB() &&
		m_RoleTitleManager.IsLoadDB() &&
		m_RoleIDEntity.IsLoadDB() &&
		m_RoleActionManager.IsLoadDB() &&
		m_RoleGiffManager.IsLoadDB() &&
		m_RoleGameData.IsLoadDB() &&
		m_RoleRank.IsLoadDB() &&
		m_RoleCharManger.IsLoadDB() && 
		m_RoleRelationRequest.IsLoadDB()
		)
	{
		//��Щ��������� ��������������� ÿ���¼���ȡ��ʱ���Ѿ�������
		UpdateRoleEvent();

		SendUserInfoToCenter();//��������ݷ��͵����������ȥ

		OnHandleRoleVersionChange();

		OnUserLoadFinish(m_LogobByGameServer);

		m_IsOnline = true;

		
		return true;
	}
	else
	{
		return false;
	}
}
void CRoleEx::OnHandleRoleVersionChange()
{
	//��Ϊ�汾����һЩ����
	//1302 1303 1304
     //10   100  300
	DWORD ChangeRate = 1;
	if (m_RoleInfo.TotalRechargeSum >= 2)
		ChangeRate = 2;

	DWORD Sum = GetItemManager().QueryItemAllTimeCount(1302);
	if (Sum > 0)
	{
		//���ӵ��3���� 
		DWORD AddCurrcey = Sum * 10 * ChangeRate;
		if (GetItemManager().OnDelUserItem(1302, GetItemManager().QueryItemCount(1302)))
		{
			//�����ʼ�
			tagRoleMail	MailInfo;
			MailInfo.bIsRead = false;
			if (ChangeRate == 2)
				_sntprintf_s(MailInfo.Context, CountArray(MailInfo.Context), TEXT("{ItemName:ItemID=%u}�� ��ȡ�����Ѿ����޸� ���˻��������ڵ�˫��������"),1302);
			else
				_sntprintf_s(MailInfo.Context, CountArray(MailInfo.Context), TEXT("{ItemName:ItemID=%u}�� ��ȡ�����Ѿ����޸� ���˻��������ڵĲ�����"), 1302);
			MailInfo.RewardID = g_FishServer.GetFishConfig().GetSystemConfig().EmailCurrceyRewardID;
			MailInfo.RewardSum = AddCurrcey;
			MailInfo.MailID = 0;
			MailInfo.SendTimeLog = time(NULL);
			MailInfo.SrcFaceID = 0;
			TCHARCopy(MailInfo.SrcNickName, CountArray(MailInfo.SrcNickName), TEXT(""), 0);
			MailInfo.SrcUserID = 0;//ϵͳ����
			MailInfo.bIsExistsReward = (MailInfo.RewardID != 0 && MailInfo.RewardSum != 0);
			DBR_Cmd_AddUserMail msg;
			SetMsgInfo(msg, DBR_AddUserMail, sizeof(DBR_Cmd_AddUserMail));
			msg.dwDestUserID = GetUserID();
			msg.MailInfo = MailInfo;
			g_FishServer.SendNetCmdToDB(&msg);
		}
	}

	Sum = GetItemManager().QueryItemAllTimeCount(1303);
	if (Sum > 0)
	{
		//���ӵ��3���� 
		DWORD AddCurrcey = Sum * 100 * ChangeRate;
		if (GetItemManager().OnDelUserItem(1303, GetItemManager().QueryItemCount(1303)))
		{
			//�����ʼ�
			tagRoleMail	MailInfo;
			MailInfo.bIsRead = false;
			if (ChangeRate == 2)
				_sntprintf_s(MailInfo.Context, CountArray(MailInfo.Context), TEXT("{ItemName:ItemID=%u}�� ��ȡ�����Ѿ����޸� ���˻��������ڵ�˫��������"), 1303);
			else
				_sntprintf_s(MailInfo.Context, CountArray(MailInfo.Context), TEXT("{ItemName:ItemID=%u}�� ��ȡ�����Ѿ����޸� ���˻��������ڵĲ�����"), 1303);
			MailInfo.RewardID = g_FishServer.GetFishConfig().GetSystemConfig().EmailCurrceyRewardID;
			MailInfo.RewardSum = AddCurrcey;
			MailInfo.MailID = 0;
			MailInfo.SendTimeLog = time(NULL);
			MailInfo.SrcFaceID = 0;
			TCHARCopy(MailInfo.SrcNickName, CountArray(MailInfo.SrcNickName), TEXT(""), 0);
			MailInfo.SrcUserID = 0;//ϵͳ����
			MailInfo.bIsExistsReward = (MailInfo.RewardID != 0 && MailInfo.RewardSum != 0);
			DBR_Cmd_AddUserMail msg;
			SetMsgInfo(msg, DBR_AddUserMail, sizeof(DBR_Cmd_AddUserMail));
			msg.dwDestUserID = GetUserID();
			msg.MailInfo = MailInfo;
			g_FishServer.SendNetCmdToDB(&msg);
		}
	}

	Sum = GetItemManager().QueryItemAllTimeCount(1304);
	if (Sum > 0)
	{
		//���ӵ��3���� 
		DWORD AddCurrcey = Sum * 300 * ChangeRate;
		if (GetItemManager().OnDelUserItem(1304, GetItemManager().QueryItemCount(1304)))
		{
			//�����ʼ�
			tagRoleMail	MailInfo;
			MailInfo.bIsRead = false;
			if (ChangeRate == 2)
				_sntprintf_s(MailInfo.Context, CountArray(MailInfo.Context), TEXT("{ItemName:ItemID=%u}�� ��ȡ�����Ѿ����޸� ���˻��������ڵ�˫��������"), 1304);
			else
				_sntprintf_s(MailInfo.Context, CountArray(MailInfo.Context), TEXT("{ItemName:ItemID=%u}�� ��ȡ�����Ѿ����޸� ���˻��������ڵĲ�����"), 1304);
			MailInfo.RewardID = g_FishServer.GetFishConfig().GetSystemConfig().EmailCurrceyRewardID;
			MailInfo.RewardSum = AddCurrcey;
			MailInfo.MailID = 0;
			MailInfo.SendTimeLog = time(NULL);
			MailInfo.SrcFaceID = 0;
			TCHARCopy(MailInfo.SrcNickName, CountArray(MailInfo.SrcNickName), TEXT(""), 0);
			MailInfo.SrcUserID = 0;//ϵͳ����
			MailInfo.bIsExistsReward = (MailInfo.RewardID != 0 && MailInfo.RewardSum != 0);
			DBR_Cmd_AddUserMail msg;
			SetMsgInfo(msg, DBR_AddUserMail, sizeof(DBR_Cmd_AddUserMail));
			msg.dwDestUserID = GetUserID();
			msg.MailInfo = MailInfo;
			g_FishServer.SendNetCmdToDB(&msg);
		}
	}
}
void CRoleEx::UpdateRoleEvent()
{
	//���� ���е����� �ɾ� � �� Join ����
	m_RoleTask.OnResetJoinAllTask();
	m_RoleActionManager.OnResetJoinAllAction();
	m_RoleAchievement.OnResetJoinAllAchievement();
}
DWORD CRoleEx::GetRoleOnlineSec()
{
	return ConvertInt64ToDWORD(m_RoleInfo.OnlineMin * 60 + (time(NULL) - m_LogonTimeByDay));//��ȡ�������ߵ���ɱ
}
void CRoleEx::ChangeRoleSocketID(DWORD SocketID)
{
	if (!m_RoleManager)
	{
		ASSERT(false);
		return;
	}
	if (m_dwGameSocketID == 0)//�����˲������޸�
	{
		ASSERT(false);
		return;
	}
	m_RoleManager->ChangeRoleSocket(this, SocketID);
	m_dwGameSocketID = SocketID;
}
void CRoleEx::OnUserLoadFinish(bool IsLogonGameServer)//����ǰ���������ϵ�ʱ��
{


	//g_FishServer.ShowInfoToWin("OnUserLoadFinish����ǰ���������ϵ�ʱ��%d,%d,%d", m_IsRobot, m_IsAfk);
	g_FishServer.GetRoleLogonManager().OnDleRoleOnlyInfo(m_RoleInfo.dwUserID);//��ҵ�½�ɹ���ʱ�� ɾ�������Logon�ϱ����Ψһ��

	//g_DBLogManager.LogRoleOnlineInfo(m_RoleInfo.dwUserID, true, SendLogDB);

	//��Ҫ��ȷ�� �����Ҫǰ�� ���� ���� �Ǳ���
	//����״̬ȷ�����
	CRole* pRole = g_FishServer.GetTableManager()->SearchUser(m_RoleInfo.dwUserID);
//	LogInfoToFile("LogGoldWanjiaLogon.txt", "��ҵ�¼��ȡ:::::::: %d:::::::::::::: %d*************", m_RoleInfo.dwGlobeNum, m_RoleInfo.dwUserID);
	bool IsToScrene = false;
	if (!IsLogonGameServer)
	{
		IsToScrene = false;
		if (pRole)
		{
			g_FishServer.GetTableManager()->OnPlayerLeaveTable(this->GetUserID());//���������������� �뿪����
		}
	}
	else
	{
		IsToScrene = (pRole != null);
	}
	SetAfkStates(false);
	if (IsToScrene && pRole)
	{
		//ǰ������
		GameTable* pTable = g_FishServer.GetTableManager()->GetTable(pRole->GetTableID());
		if (pTable)
		{
			//����ǰ�������ݷ��͵��ͻ���ȥ
			//1.������������� ������Ҫ��������ͻ���ȥ 
			LC_Cmd_AccountLogonToScreen msg;
			SetMsgInfo(msg, GetMsgType(Main_Logon, LC_AccountLogonToScreen), sizeof(LC_Cmd_AccountLogonToScreen));
			msg.RandID = g_FishServer.GetRoleLogonManager().OnAddRoleOnlyInfo(m_RoleInfo.dwUserID);//�������ӵ�Ψһ��������ȥ
			msg.bTableTypeID = pTable->GetTableTypeID();
			msg.BackgroundImage = pTable->GetFishDesk()->GetSceneBackground();
			if (m_RoleLauncherManager.IsCanUserLauncherByID(pRole->GetLauncherType()))//����ʹ�õ�ǰ����
				msg.LauncherType = pRole->GetLauncherType() | 128;
			else
				msg.LauncherType = pRole->GetLauncherType();
			msg.SeatID = pRole->GetSeatID();
			msg.RateIndex = pRole->GetRateIndex();
			msg.Energy = pRole->GetEnergy();
			SendDataToClient(&msg);
			//���������ϵ����ݵ��ͻ���ȥ
			ResetRoleInfoToClient();//ˢ�������ϵ�����

			LogInfoToFile("LogGoldWanjiaLogon.txt", "������������� ::::::::%d********** %d------", m_RoleInfo.dwGlobeNum, m_RoleInfo.dwUserID);
		}
		else
		{
			//��Ҳ�����������
			if (IsLogonGameServer)
				ResetClientInfo();

			LC_Cmd_AccountOnlyIDSuccess msg;
			SetMsgInfo(msg, GetMsgType(Main_Logon, LC_AccountOnlyIDSuccess), sizeof(LC_Cmd_AccountOnlyIDSuccess));
			msg.RandID = g_FishServer.GetRoleLogonManager().OnAddRoleOnlyInfo(m_RoleInfo.dwUserID);//�������ӵ�Ψһ��������ȥ
			msg.RoleInfo = m_RoleInfo;
			msg.OperateIp = g_FishServer.GetOperateIP();
			SendDataToClient(&msg);
			LogInfoToFile("LogGoldWanjiaLogon.txt", "��Ҳ����������� ::::::%d ::::::%d*******", m_RoleInfo.dwGlobeNum, m_RoleInfo.dwUserID);
		}
	}
	else
	{
		if (IsLogonGameServer)
			ResetClientInfo();
		//ǰ������
		LC_Cmd_AccountOnlyIDSuccess msg;
		SetMsgInfo(msg, GetMsgType(Main_Logon, LC_AccountOnlyIDSuccess), sizeof(LC_Cmd_AccountOnlyIDSuccess));
		msg.RandID = g_FishServer.GetRoleLogonManager().OnAddRoleOnlyInfo(m_RoleInfo.dwUserID);//�������ӵ�Ψһ��������ȥ
		msg.RoleInfo = m_RoleInfo;
		msg.OperateIp = g_FishServer.GetOperateIP();
		SendDataToClient(&msg);
	//	LogInfoToFile("LogGoldWanjiaLogon.txt", "ǰ������:::: %d::::: %d******", m_RoleInfo.dwGlobeNum, m_RoleInfo.dwUserID);
	}
	//�ⲿ����������
	SYSTEMTIME time;
	GetLocalTime(&time);
	LC_Cmd_DayChange msgSystemTime;
	SetMsgInfo(msgSystemTime, GetMsgType(Main_Role, LC_DayChange), sizeof(LC_Cmd_DayChange));
	msgSystemTime.Year = (BYTE)(time.wYear - 2000);
	msgSystemTime.Month = (BYTE)time.wMonth;
	msgSystemTime.Day = (BYTE)time.wDay;
	msgSystemTime.Hour = (BYTE)time.wHour;
	msgSystemTime.Min = (BYTE)time.wMinute;
	msgSystemTime.Sec = (BYTE)time.wSecond;
	msgSystemTime.IsDayUpdate = false;
	//msgSystemTime.IsNewDay = false;
	SendDataToClient(&msgSystemTime);

	//��������������� ���͵��ͻ���ȥ
	g_FishServer.SendAllMonthPlayerSumToClient(m_RoleInfo.dwUserID);
	m_RoleLauncherManager.OnInit(this);
	m_RoleMessageStates.OnInit(this);//��ҵ�½�ɹ���������


	//�ж�����Ƿ���Ҫ�������ö�������
	if (GetRoleEntity().GetEntityInfo().Phone != 0 && m_RoleServerInfo.SecPasswordCrc1 == 0 && m_RoleServerInfo.SecPasswordCrc2 == 0 && m_RoleServerInfo.SecPasswordCrc3 == 0)
	{
		//����Ѿ������ֻ� ����δ���� �ֻ��Ķ������� ������Ҫ����������ֻ�����
		LC_Cmd_SetSecondPassword msg;
		SetMsgInfo(msg, GetMsgType(Main_Role, LC_SetSecondPassword), sizeof(LC_Cmd_SetSecondPassword));
		SendDataToClient(&msg);
	}


	if (m_IsRobot)
		g_FishServer.GetRobotManager().OnAddRobot(GetUserID());//�����ǰ���Ϊ������ ���뵽�����˵ļ�������ȥ


///	LogInfoToFile("LogGoldWanjiaLogon.txt", "��½�ɹ� ����������%d ::::::::::%d******", m_RoleInfo.dwGlobeNum, m_RoleInfo.dwUserID);

	//char IpStr[32] = { 0 };
	//sprintf_s(IpStr, sizeof(IpStr), "��� %d ��½�ɹ�\n", m_RoleInfo.dwUserID);
	//std::cout << IpStr;

	return;
}
//bool CRoleEx::SetRoleIsOnline(bool IsOnline)
//{
//	//�������ݿ����� ���õ�ǰ����Ѿ�������
//	DBR_Cmd_RoleOnline msg;
//	SetMsgInfo(msg,DBR_SetOnline, sizeof(DBR_Cmd_RoleOnline));
//	msg.dwUserID = m_RoleInfo.dwUserID;
//	msg.IsOnline = IsOnline;
//	g_FishServer.SendNetCmdToSaveDB(&msg);
//	return true;
//}
void CRoleEx::SendDataToClient(NetCmd* pCmd)
{
	//g_FishServer.ShowInfoToWin("SendDataToClient������Ϣ���ͻ���%d,%d,%d",m_IsRobot, m_IsAfk);
	if (!pCmd)
	{
		ASSERT(false);
		return;
	}
	if (m_IsRobot)
		return;
	if (m_IsAfk)//��������� ���뷢������ͻ���ȥ
		return;
	//����ҵ�����͵��ͻ���ȥ ���ҽ����ݽ��д���
	ServerClientData* pClient = g_FishServer.GetUserClientDataByIndex(m_dwGameSocketID);
	if (!pClient)
		return;
	g_FishServer.SendNewCmdToClient(pClient, pCmd);
}
//�˴����������Ϣ
void CRoleEx::SendUserInfoToCenter()
{
	//g_FishServer.ShowInfoToWin("SendUserInfoToCenter�˴����������Ϣ");

	//����ǰ��ҵ����ݷ��͵�Centerȥ
	//1.������ҵĻ�������
	CL_UserEnter msg;
	SetMsgInfo(msg,GetMsgType(Main_Center, CL_Sub_UserEnter), sizeof(CL_UserEnter));
	msg.RoleInfo.bGender = m_RoleInfo.bGender;
	//msg.RoleInfo.dwExp = m_RoleInfo.dwExp;
	msg.RoleInfo.dwFaceID = m_RoleInfo.dwFaceID;
	msg.RoleInfo.dwUserID = m_RoleInfo.dwUserID;
	msg.RoleInfo.wLevel = m_RoleInfo.wLevel;//��ҵȼ�
	msg.RoleInfo.dwAchievementPoint = m_RoleInfo.dwAchievementPoint;
	msg.RoleInfo.TitleID = m_RoleInfo.TitleID;
	msg.RoleInfo.ClientIP = m_RoleInfo.ClientIP;//�ͻ��˵�IP��ַ
	msg.IsRobot = m_IsRobot;
	msg.RoleInfo.IsShowIpAddress = m_RoleInfo.IsShowIPAddress;
	msg.RoleInfo.VipLevel = 5;//m_RoleInfo.VipLevel;//���vip
	msg.RoleInfo.IsInMonthCard = (m_RoleInfo.MonthCardID != 0 && time(null) <= m_RoleInfo.MonthCardEndTime);
	msg.RoleInfo.ParticularStates = m_RoleInfo.ParticularStates;//����״̬
	msg.RoleInfo.GameID = m_RoleInfo.GameID;
	for (int i = 0; i < MAX_CHARM_ITEMSUM;++i)
		msg.RoleInfo.CharmArray[i] = m_RoleInfo.CharmArray[i];
	TCHARCopy(msg.RoleInfo.NickName, CountArray(msg.RoleInfo.NickName), m_RoleInfo.NickName, _tcslen(m_RoleInfo.NickName));
	msg.RoleInfo.IsOnline = true;//������������������״̬Ϊ����״̬
	SendDataToCenter(&msg);
	//2.������ҵĹ�ϵ����
	m_RelationManager.SendRoleRelationToCenter();//�����ݷ��͵�Centerȥ
	//3.
	CL_UserEnterFinish msgFinish;
	SetMsgInfo(msgFinish,GetMsgType(Main_Center, CL_Sub_UserEnterFinish), sizeof(CL_UserEnterFinish));
	msgFinish.dwUserID = m_RoleInfo.dwUserID;
	SendDataToCenter(&msgFinish);
	//4.
	if (m_IsChangeClientIP)
	{
		//��ҵ�IP��ַ�仯�� ������Ҫ�����޸���ҵ�IP��ַ ����֪ͨ������� ����Ϸ������IP��ַ�ǲ��ᷢ���仯��
		CC_Cmd_ChangeRoleClientIP msg;
		SetMsgInfo(msg, GetMsgType(Main_Role, CC_ChangeRoleClientIP), sizeof(CC_Cmd_ChangeRoleClientIP));
		msg.dwUserID = m_RoleInfo.dwUserID;
		msg.ClientIP = m_RoleInfo.ClientIP;//���IP�仯��
		m_IsChangeClientIP = false;
	}
}
void CRoleEx::SendUserLeaveToCenter()
{
//	g_FishServer.ShowInfoToWin("SendUserLeaveToCenter����뿪���������");
	//����뿪���������
	//g_FishServer.DelRoleOnlineInfo(m_RoleInfo.dwUserID);
	CL_UserLeave msg;
	SetMsgInfo(msg,GetMsgType(Main_Center, CL_Sub_UserLeave), sizeof(CL_UserLeave));
	msg.dwUserID = m_RoleInfo.dwUserID;
	SendDataToCenter(&msg);
}
void CRoleEx::SendDataToCenter(NetCmd* pCmd)
{
	g_FishServer.SendNetCmdToCenter(pCmd);
}
//void CRoleEx::SendDataToRank(NetCmd* pCmd)
//{
//	g_FishServer.SendNetCmdToRank(pCmd);
//}
//�޸ĺ��� ������������Լ��� �й�ϵ(��Ҫ����ͬ��) ������ (���� ��Ҫ����ͬ��)
bool CRoleEx::ChangeRoleExp(int AddExp, bool IsSendToClient)
{
	//g_FishServer.ShowInfoToWin("ChangeRoleExp�޸ĺ���");
	if (!g_FishServer.GetTableManager() || !g_FishServer.GetTableManager()->GetGameConfig())
	{
		ASSERT(false);
		return false;
	}
	if (AddExp == 0)
		return true;
	if (!CheckChangeDWORDValue(m_RoleInfo.dwExp, AddExp))
		return false;
	m_RoleInfo.dwExp += AddExp;
	//��ҵľ���������  ������Ҫ �ж�����Ƿ����ȼ��޸ĵ�״̬  ����ȼ�Ҳ�仯�� ������Ҫ���������  �ȼ�����
	DWORD ChangeExp = 0;
	WORD  ChangeLevel = 0;
	g_FishServer.GetTableManager()->GetGameConfig()->LevelUp(m_RoleInfo.wLevel, m_RoleInfo.dwExp, ChangeLevel, ChangeExp);
	if (ChangeExp != m_RoleInfo.dwExp)
		m_RoleInfo.dwExp = ChangeExp;
	if (IsSendToClient || (m_RoleInfo.wLevel != ChangeLevel))//���ȼ����ͱ仯��ʱ�� ���;��鵽�ͻ���ȥ
	{
		LC_Cmd_ChangRoleExp msg;
		SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleExp), sizeof(LC_Cmd_ChangRoleExp));
		msg.dwExp = ChangeExp;
		SendDataToClient(&msg);
	}
	m_IsNeedSave = true;
	if (m_RoleInfo.wLevel != ChangeLevel)
	{
		ChangeRoleLevel(ChangeLevel - m_RoleInfo.wLevel);
	}
	
	return true;
}
bool CRoleEx::ChangeRoleLevel(short AddLevel)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleLevel�޸���ҵȼ�");
	//����ҵ���ı��ʱ��
	if (AddLevel == 0)
		return true;
	if (!CheckChangeDWORDValue(m_RoleInfo.wLevel, AddLevel))
		return false;
	m_RoleInfo.wLevel += AddLevel;

	OnHandleEvent(true,true,true,ET_UpperLevel, 0, AddLevel);
	OnHandleEvent(true, true, true, ET_ToLevel, 0,m_RoleInfo.wLevel);
	//����ҵȼ��仯��ʱ��
	m_RoleTask.OnRoleLevelChange();
	m_RoleAchievement.OnRoleLevelChange();
	m_RoleActionManager.OnRoleLevelChange();

	HashMap<WORD, WORD>::iterator Iter = g_FishServer.GetFishConfig().GetFishLevelRewardConfig().m_LevelRewardMap.find(m_RoleInfo.wLevel);
	if (Iter != g_FishServer.GetFishConfig().GetFishLevelRewardConfig().m_LevelRewardMap.end())
	{
		WORD RewardID = Iter->second;
		OnAddRoleRewardByRewardID(RewardID, TEXT("������ý���"));
	}

	LC_Cmd_ChangeRoleLevel msg;
	SetMsgInfo(msg,GetMsgType(Main_Role, LC_ChangeRoleLevel), sizeof(LC_Cmd_ChangeRoleLevel));
	msg.wLevel = m_RoleInfo.wLevel;
	//������ȥ ���� �Ѿ� ȫ���������ϵ����
	SendDataToClient(&msg);

	LC_Cmd_TableChangeRoleLevel msgTable;
	SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleLevel), sizeof(LC_Cmd_TableChangeRoleLevel));
	msgTable.dwDestUserID = m_RoleInfo.dwUserID;
	msgTable.wLevel = m_RoleInfo.wLevel;
	SendDataToTable(&msgTable);

	CC_Cmd_ChangeRoleLevel msgCenter;
	SetMsgInfo(msgCenter,GetMsgType(Main_Role, CC_ChangeRoleLevel), sizeof(CC_Cmd_ChangeRoleLevel));
	msgCenter.dwUserID = m_RoleInfo.dwUserID;
	msgCenter.wLevel = m_RoleInfo.wLevel;
	SendDataToCenter(&msgCenter);


	DBR_Cmd_SaveRoleLevel msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleLevel, sizeof(DBR_Cmd_SaveRoleLevel));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.wLevel = m_RoleInfo.wLevel;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);
	return true;
}
bool CRoleEx::ChangeRoleGender(bool bGender)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleGender����Ա�");
	if (m_RoleInfo.bGender == bGender)
		return true;
	m_RoleInfo.bGender = bGender;
	LC_Cmd_ChangeRoleGender msg;
	SetMsgInfo(msg,GetMsgType(Main_Role, LC_ChangeRoleGender), sizeof(LC_Cmd_ChangeRoleGender));
	msg.bGender = bGender;
	SendDataToClient(&msg);

	LC_Cmd_TableChangeRoleGender msgTable;
	SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleGender), sizeof(LC_Cmd_TableChangeRoleGender));
	msgTable.dwDestUserID = m_RoleInfo.dwUserID;
	msgTable.bGender = bGender;
	SendDataToTable(&msgTable);

	CC_Cmd_ChangeRoleGender msgCenter;
	SetMsgInfo(msgCenter,GetMsgType(Main_Role, CC_ChangeRoleGender), sizeof(CC_Cmd_ChangeRoleGender));
	msgCenter.dwUserID = m_RoleInfo.dwUserID;
	msgCenter.bGender = bGender;
	SendDataToCenter(&msgCenter);

	DBR_Cmd_SaveRoleGender msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleGender, sizeof(DBR_Cmd_SaveRoleGender));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.bGender = m_RoleInfo.bGender;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);


	return true;
}
bool CRoleEx::ChangeRoleFaceID(DWORD FaceID)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleFaceID���ͷ��");
	if (m_RoleInfo.dwFaceID == FaceID)
		return true;
	m_RoleInfo.dwFaceID = FaceID;
	LC_Cmd_ChangeRoleFaceID msg;
	SetMsgInfo(msg,GetMsgType(Main_Role, LC_ChangeRoleFaceID), sizeof(LC_Cmd_ChangeRoleFaceID));
	msg.dwFaceID = m_RoleInfo.dwFaceID;
	SendDataToClient(&msg);

	LC_Cmd_TableChangeRoleFaceID msgTable;
	SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleFaceID), sizeof(LC_Cmd_TableChangeRoleFaceID));
	msgTable.dwDestUserID = m_RoleInfo.dwUserID;
	msgTable.dwDestFaceID = m_RoleInfo.dwFaceID;
	SendDataToTable(&msgTable);

	CC_Cmd_ChangeRoleFaceID msgCenter;
	SetMsgInfo(msgCenter,GetMsgType(Main_Role, CC_ChangeRoleFaceID), sizeof(CC_Cmd_ChangeRoleFaceID));
	msgCenter.dwUserID = m_RoleInfo.dwUserID;
	msgCenter.dwFaceID = m_RoleInfo.dwFaceID;
	SendDataToCenter(&msgCenter);

	GM_Cmd_RoleChangeFaceID msgMini;
	SetMsgInfo(msgMini, GetMsgType(Main_MiniGame, GM_RoleChangeFaceID), sizeof(GM_Cmd_RoleChangeFaceID));
	msgMini.dwUserID = m_RoleInfo.dwUserID;
	msgMini.FaceID = m_RoleInfo.dwFaceID;
	g_FishServer.SendNetCmdToMiniGame(&msgMini);

	DBR_Cmd_SaveRoleFaceID msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleFaceID, sizeof(DBR_Cmd_SaveRoleFaceID));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.dwFaceID = m_RoleInfo.dwFaceID;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ChangeRoleNickName(LPTSTR pNickName)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleNickName��ҽ�������");
	if (_tcscmp(pNickName, m_RoleInfo.NickName) == 0)
		return true;
	if (!g_FishServer.GetFishConfig().CheckStringIsError(pNickName, MIN_NICKNAME, MAX_NICKNAME, SCT_Normal))
	{
		ASSERT(false);
		return false;
	}
	//��ҽ������� �޸� ��Ҫ��ѯ�����ݿ� 
	DBR_Cmd_SaveRoleNickName msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleNickName, sizeof(DBR_Cmd_SaveRoleNickName));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	TCHARCopy(msgDB.NickName, CountArray(msgDB.NickName), pNickName, _tcslen(pNickName));
	g_FishServer.SendNetCmdToDB(&msgDB);
	return true;
}
void CRoleEx::ChangeRoleNickNameResult(DBO_Cmd_SaveRoleNickName* pMsg)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleNickNameResult�޸��ǳ�");
	if (!pMsg)
	{
		ASSERT(false);
		return;
	}
	if (pMsg->Result)
	{
		TCHARCopy(m_RoleInfo.NickName, CountArray(m_RoleInfo.NickName), pMsg->NickName, _tcslen(pMsg->NickName));

		LC_Cmd_ChangeRoleNickName msg;
		msg.Result = true;
		SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleNickName), sizeof(LC_Cmd_ChangeRoleNickName));
		TCHARCopy(msg.NickName, CountArray(msg.NickName), pMsg->NickName, _tcslen(pMsg->NickName));
		SendDataToClient(&msg);

		LC_Cmd_TableChangeRoleNickName msgTable;
		SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleNickName), sizeof(LC_Cmd_TableChangeRoleNickName));
		msgTable.dwDestUserID = m_RoleInfo.dwUserID;
		TCHARCopy(msgTable.NickName, CountArray(msgTable.NickName), pMsg->NickName, _tcslen(pMsg->NickName));
		SendDataToTable(&msgTable);

		CC_Cmd_ChangeRoleNickName msgCenter;
		SetMsgInfo(msgCenter, GetMsgType(Main_Role, CC_ChangeRoleNickName), sizeof(CC_Cmd_ChangeRoleNickName));
		msgCenter.dwUserID = m_RoleInfo.dwUserID;
		TCHARCopy(msgCenter.NickName, CountArray(msgCenter.NickName), pMsg->NickName, _tcslen(pMsg->NickName));
		SendDataToCenter(&msgCenter);

		GM_Cmd_RoleChangeNickName msgMini;
		SetMsgInfo(msgMini, GetMsgType(Main_MiniGame, GM_RoleChangeNickName), sizeof(GM_Cmd_RoleChangeNickName));
		msgMini.dwUserID = m_RoleInfo.dwUserID;
		TCHARCopy(msgMini.NickName, CountArray(msgMini.NickName), pMsg->NickName, _tcslen(pMsg->NickName));
		g_FishServer.SendNetCmdToMiniGame(&msgMini);
	}
	else
	{
		//�޸�ʧ����
		LC_Cmd_ChangeRoleNickName msg;
		msg.Result = false;
		SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleNickName), sizeof(LC_Cmd_ChangeRoleNickName));
		TCHARCopy(msg.NickName, CountArray(msg.NickName), pMsg->NickName, _tcslen(pMsg->NickName));
		SendDataToClient(&msg);
	}
}
//��ӿͻ�����֤
bool CRoleEx::CheckUserBIsDaili(int UserGlobe, bool IsSendToClient , bool IsSaveToDB , bool IsSendToMiniGame , bool bIsCanContrl )
{
	//if (IsSendToClient)
	//{

	//	LC_Cmd_CheckClientInfo msg;
	//	SetMsgInfo(msg, GetMsgType(Main_Role, LC_CheckClientInfo), sizeof(LC_Cmd_CheckClientInfo));
	//	msg.Result = DownGlobe;//m_RoleInfo.dwGlobeNum;
	//	SendDataToClient(&msg);//ֻ���Ϳͻ���ȥ


	//}


	//LC_Cmd_CheckClientInfo msg;
	//SetMsgInfo(msg, GetMsgType(Main_Control, LC_CheckClientInfo), sizeof(LC_Cmd_CheckClientInfo));
	//msg.Result = (pMsg->RankValue == 000000 && bvaliddmac);//��½�ɹ��� ���Խ���m_ControlRankValue
	////FishServer::SendNetCmdToClient(pClient, &msg);

	return true;
}

//���  ���ٽ�Һ���  DownGlobe����������
bool CRoleEx::ChangeRoleGlobeDown(int DownGlobe, bool IsSendToClient, bool IsSaveToDB, bool IsSendToMiniGame, bool bIsCanContrl )
{
	if (!bIsCanContrl)
	{
		return true;
	}
	if (DownGlobe == 0)
		return true;
	if ((m_RoleInfo.dwGlobeNum - DownGlobe)<=0)
	{
		m_RoleInfo.dwGlobeNum = 0;
		//return true;
	}
	else
	{
		m_RoleInfo.dwGlobeNum -= DownGlobe;
	}
	LogInfoToFile("CollideBuYu.txt", "LC_ChangeRoleGlobeDown�޸���ҽ�Ҽ��ټ��ٽ�Һ�������������ô����:::::::::  %d =======::%d***********%d-----****%d******%d***----", m_RoleInfo.dwGlobeNum, DownGlobe, m_RoleInfo.dwUserID, IsSendToClient, IsSaveToDB);
	//OnHandleEvent(true, true, true, ET_GetGlobel, 0, DownGlobe);

	//OnHandleEvent(true, true, true, ET_MaxGlobelSum, 0, m_RoleInfo.dwGlobeNum);

	if (IsSendToClient)
	{
		
		LC_Cmd_ChangeRoleGlobeDown msg;
		SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleGlobeDown), sizeof(LC_Cmd_ChangeRoleGlobeDown));
		msg.dwGlobeNum = DownGlobe;//m_RoleInfo.dwGlobeNum;
		SendDataToClient(&msg);//ֻ���Ϳͻ���ȥ

		LC_Cmd_TableChangeRoleGlobeDown msgTable;
		SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleGlobeDown), sizeof(LC_Cmd_TableChangeRoleGlobeDown));
		msgTable.dwDestUserID = m_RoleInfo.dwUserID;
		msgTable.dwGlobelSum = DownGlobe;//m_RoleInfo.dwGlobeNum;
		SendDataToTable(&msgTable);

		LogInfoToFile("CollideBuYu.txt", "LC_ChangeRoleGlobeDown�޸���ҽ�Ҽ��ټ��ٽ�Һ������͵��ͻ���:::::::::  %d =======::%d***************%d---------", m_RoleInfo.dwGlobeNum, DownGlobe, m_RoleInfo.dwUserID);
	}

	if (IsSaveToDB)
	{
		//	g_FishServer.ShowInfoToWin("ChangeRoleGlobe���浽���ݿ�ȥ");
		//���浽���ݿ�ȥ
		DBR_Cmd_SaveRoleGlobelDown msgDB;//����Ľ����Ϣ
		SetMsgInfo(msgDB, DBR_SaveRoleGlobelDown, sizeof(DBR_Cmd_SaveRoleGlobelDown));
	
			msgDB.dwDaiLiUserID = 0;
			msgDB.bIsCanContrl = bIsCanContrl;
			msgDB.dwAddGlobel = DownGlobe;
	
		//msgDB.dwAddGlobel = AddGlobe;

		msgDB.dwUserID = m_RoleInfo.dwUserID;
		msgDB.dwGlobel = m_RoleInfo.dwGlobeNum;
		//g_FishServer.SendNetCmdToSaveDB(&msgDB);

		LogInfoToFile("CollideBuYu.txt", "LC_ChangeRoleGlobeDown�޸���ҽ�Ҽ��ټ��ٽ�Һ������͵����ݿ�:::::::::  %d =======::%d***************%d---------", m_RoleInfo.dwGlobeNum, DownGlobe, m_RoleInfo.dwUserID);
	}
	else
		m_IsNeedSave = true;

	if (IsSendToMiniGame)
	{
		GM_Cmd_RoleChangeGlobel msg;
		SetMsgInfo(msg, GetMsgType(Main_MiniGame, GM_RoleChangeGlobel), sizeof(GM_Cmd_RoleChangeGlobel));
		msg.dwUserID = m_RoleInfo.dwUserID;
		msg.dwGlobel = m_RoleInfo.dwGlobeNum;
		g_FishServer.SendNetCmdToMiniGame(&msg);
	}

	//m_RoleGameData.OnHandleRoleGetGlobelDown(DownGlobe);//��Ҽ��ٽ��

	LogInfoToFile("CollideBuYu.txt", "ChangeRoleGlobeDown�޸���ҽ�Ҽ��ټ��ٽ�Һ���:::::::::  %d =======%d***************%d---------", m_RoleInfo.dwGlobeNum, DownGlobe, m_RoleInfo.dwUserID);
	return true;
}

//	��ұ仯		ǩ��  ��ֵ 
bool CRoleEx::ChangeRoleGlobe(int AddGlobe, bool IsSendToClient, bool IsSaveToDB, bool IsSendToMiniGame, DWORD dwDaiLiUserID, bool bIsCanContrl,bool bIsZongDai)
{
	//LogInfoToFile("CollideBuYu.txt", "ChangeRoleGlobe�޸���ҽ�ұ���֮ǰ��ұ仯***********  %d =======%d***************%d------:%d---", m_RoleInfo.dwGlobeNum, AddGlobe, m_RoleInfo.dwUserID, g_FishServer.GetFishConfig().GetSystemConfig().MaxGobelSum);
	//g_FishServer.ShowInfoToWin("ChangeRoleGlobe�޸���ҽ��");
	if (AddGlobe == 0)
		return true;
	if (!CheckChangeDWORDValue(m_RoleInfo.dwGlobeNum, AddGlobe))
		return false;
	//LogInfoToFile("CollideBuYu.txt", "ChangeRoleGlobe�޸�ǰ��ұ仯���������111***********  %d =======%d***********%d-----****----", m_RoleInfo.dwGlobeNum, AddGlobe, m_RoleInfo.dwUserID);
	if (AddGlobe >0 && m_RoleInfo.dwGlobeNum + AddGlobe >= g_FishServer.GetFishConfig().GetSystemConfig().MaxGobelSum)//��ҵ��������� �޷���ӽ��
		return false;
	//if (AddGlobe<0)
	//{

	//}
	//LogInfoToFile("CollideBuYu.txt", "ChangeRoleGlobe�޸�ǰ��ұ仯���������2222222222***********  %d =======%d***********%d---------", m_RoleInfo.dwGlobeNum, AddGlobe, m_RoleInfo.dwUserID);
	m_RoleInfo.dwGlobeNum += AddGlobe;


	if (AddGlobe > 0)
		OnHandleEvent(true, true, true, ET_GetGlobel, 0, AddGlobe);

	OnHandleEvent(true, true, true, ET_MaxGlobelSum, 0,m_RoleInfo.dwGlobeNum);
	//LogInfoToFile("CollideBuYu.txt", "ChangeRoleGlobe�޸�ǰ��ұ仯���������3333333***********  %d =======%d***********%d---------", m_RoleInfo.dwGlobeNum, AddGlobe, m_RoleInfo.dwUserID);
	if (IsSendToClient)
	{
		//g_FishServer.ShowInfoToWin("ChangeRoleGlobeֻ���Ϳͻ���ȥ");
		LC_Cmd_ChangeRoleGlobe msg;
		SetMsgInfo(msg,GetMsgType(Main_Role, LC_ChangeRoleGlobe), sizeof(LC_Cmd_ChangeRoleGlobe));
		msg.dwGlobeNum = AddGlobe;//m_RoleInfo.dwGlobeNum;
		SendDataToClient(&msg);//ֻ���Ϳͻ���ȥ
		//LogInfoToFile("CollideBuYu.txt", "ChangeRoleGlobe�޸�ǰ��ұ仯���������44444444***********  %d =======%d***********%d---------", m_RoleInfo.dwGlobeNum, AddGlobe, m_RoleInfo.dwUserID);

		LC_Cmd_TableChangeRoleGlobe msgTable;
		SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleGlobe), sizeof(LC_Cmd_TableChangeRoleGlobe));
		msgTable.dwDestUserID = m_RoleInfo.dwUserID;
		msgTable.dwGlobelSum = AddGlobe;//m_RoleInfo.dwGlobeNum;
		SendDataToTable(&msgTable);		

	}
	//LogInfoToFile("CollideBuYu.txt", "ChangeRoleGlobe�޸���ҽ�ұ���֮ǰ####:::::::::  %d =======%d***************%d---------", m_RoleInfo.dwGlobeNum, AddGlobe, m_RoleInfo.dwUserID);
	if (IsSaveToDB)
	{
	//	g_FishServer.ShowInfoToWin("ChangeRoleGlobe���浽���ݿ�ȥ");
		//���浽���ݿ�ȥ
		DBR_Cmd_SaveRoleGlobel msgDB;
		SetMsgInfo(msgDB, DBR_SaveRoleGlobel, sizeof(DBR_Cmd_SaveRoleGlobel));
		if (dwDaiLiUserID!=0)//�������0  �Ǿ��Ǵ����ֵ
		{
			msgDB.dwDaiLiUserID = dwDaiLiUserID;
			msgDB.bIsCanContrl = bIsCanContrl;
			msgDB.dwAddGlobel = AddGlobe;
		}
		else
		{
			msgDB.dwDaiLiUserID = 0;
			msgDB.bIsCanContrl = false;
			msgDB.dwAddGlobel = 0;

		}
		//msgDB.dwAddGlobel = AddGlobe;

		msgDB.dwUserID = m_RoleInfo.dwUserID;
		msgDB.dwGlobel = m_RoleInfo.dwGlobeNum;
		g_FishServer.SendNetCmdToSaveDB(&msgDB);
	}
	else
		m_IsNeedSave = true;

	if (IsSendToMiniGame)
	{
		GM_Cmd_RoleChangeGlobel msg;
		SetMsgInfo(msg, GetMsgType(Main_MiniGame, GM_RoleChangeGlobel), sizeof(GM_Cmd_RoleChangeGlobel));
		msg.dwUserID = m_RoleInfo.dwUserID;
		msg.dwGlobel = m_RoleInfo.dwGlobeNum;
		g_FishServer.SendNetCmdToMiniGame(&msg);
	}

	if (AddGlobe > 0)
	{
		m_RoleGameData.OnHandleRoleGetGlobel(AddGlobe);
	}
	//LogInfoToFile("CollideBuYu.txt", "ChangeRoleGlobe�޸���ҽ��:::::::::  %d =======%d***************%d---------", m_RoleInfo.dwGlobeNum, AddGlobe, m_RoleInfo.dwUserID);

	return true;
}
bool CRoleEx::ChangeRoleMedal(int AddMedal, const TCHAR *pcStr)
{
	//g_FishServer.ShowInfoToWin("ChangeRoleMedal�޸���ҽ���");
	if (AddMedal == 0)
		return true;
	if (!CheckChangeDWORDValue(m_RoleInfo.dwMedalNum, AddMedal))
		return false;
	m_RoleInfo.dwMedalNum += AddMedal;
	if (AddMedal > 0)
		OnHandleEvent(true, true, true, ET_GetMadel, 0, AddMedal);

	if (AddMedal < 0)
		m_RoleInfo.TotalUseMedal += (AddMedal*-1);//��¼�����ʹ�õĽ�����

	LC_Cmd_ChangeRoleMedal msg;
	SetMsgInfo(msg,GetMsgType(Main_Role, LC_ChangeRoleMedal), sizeof(LC_Cmd_ChangeRoleMedal));
	msg.dwMedalNum = m_RoleInfo.dwMedalNum;
	msg.TotalUseMedal = m_RoleInfo.TotalUseMedal;
	SendDataToClient(&msg);//ֻ���Ϳͻ���ȥ

	GM_Cmd_RoleChangeMadel msgMini;
	SetMsgInfo(msgMini, GetMsgType(Main_MiniGame, GM_RoleChangeMadel), sizeof(GM_Cmd_RoleChangeMadel));
	msgMini.dwUserID = m_RoleInfo.dwUserID;
	msgMini.dwMadel = m_RoleInfo.dwMedalNum;
	g_FishServer.SendNetCmdToMiniGame(&msgMini);

	//���浽���ݿ�ȥ
	DBR_Cmd_SaveRoleMedal msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleMedal, sizeof(DBR_Cmd_SaveRoleMedal));
	msgDB.dwMedalSum = m_RoleInfo.dwMedalNum;
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.TotalUseMedal = m_RoleInfo.TotalUseMedal;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	//��¼Log�����ݿ�ȥ
	g_DBLogManager.LogToDB(GetUserID(), LT_Medal, AddMedal,0, pcStr, SendLogDB);
	return true;
}
bool CRoleEx::ChangeRoleCurrency(int AddCurrency, const TCHAR *pcStr)
{
	//g_FishServer.ShowInfoToWin("ChangeRoleCurrency�޸���ʯ");
	if (AddCurrency == 0)
		return true;
	if (!CheckChangeDWORDValue(m_RoleInfo.dwCurrencyNum, AddCurrency))
		return false;
	m_RoleInfo.dwCurrencyNum += AddCurrency;
	if (AddCurrency > 0)
		OnHandleEvent(true, true, true, ET_GetCurren, 0, AddCurrency);

	OnHandleEvent(true, true, true, ET_MaxCurren, 0, m_RoleInfo.dwCurrencyNum);

	LC_Cmd_ChangeRoleCurrency msg;
	SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleCurrency), sizeof(LC_Cmd_ChangeRoleCurrency));
	msg.dwCurrencyNum = m_RoleInfo.dwCurrencyNum;
	SendDataToClient(&msg);//ֻ���Ϳͻ���ȥ

	GM_Cmd_RoleChangeCurrcey msgMini;
	SetMsgInfo(msgMini, GetMsgType(Main_MiniGame, GM_RoleChangeCurrcey), sizeof(GM_Cmd_RoleChangeCurrcey));
	msgMini.dwUserID = m_RoleInfo.dwUserID;
	msgMini.dwCurrcey = m_RoleInfo.dwCurrencyNum;
	g_FishServer.SendNetCmdToMiniGame(&msgMini);

	DBR_Cmd_SaveRoleCurrency msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleCurrency, sizeof(DBR_Cmd_SaveRoleCurrency));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.dwCurrencyNum = m_RoleInfo.dwCurrencyNum;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	//��¼Log�����ݿ�ȥ
	g_DBLogManager.LogToDB(GetUserID(), LT_Currcey, AddCurrency,0, pcStr, SendLogDB);

	return true;
}
bool CRoleEx::LostUserMoney(DWORD Globel, DWORD Medal, DWORD Currey, const TCHAR *pcStr)
{
	if (Globel >= MAXINT32 || Medal >= MAXINT32 || Currey >= MAXINT32)
	{
		return false;
	}
	if (m_RoleInfo.dwGlobeNum < Globel || m_RoleInfo.dwMedalNum < Medal || m_RoleInfo.dwCurrencyNum < Currey)
		return false;
	if (!ChangeRoleGlobe(Globel*-1, true))
		return false;

	if (!ChangeRoleMedal(Medal*-1, pcStr))
	{
		ChangeRoleGlobe(Globel, true);
		return false;
	}
	if (!ChangeRoleCurrency(Currey*-1, pcStr))
	{
		ChangeRoleGlobe(Globel, true);

		//�黹�۳����ֱ�
		TCHAR NewChar[DB_Log_Str_Length];
		_stprintf_s(NewChar, CountArray(NewChar), TEXT("�黹�۳��Ľ��� ����:%d"),Medal);

		ChangeRoleMedal(Medal, NewChar);
		return false;
	}
	return true;
}
void CRoleEx::SendDataToTable(NetCmd* pCmd)
{
	if (!g_FishServer.GetTableManager())
	{
		free(pCmd);
		ASSERT(false);
		return;
	}
	g_FishServer.GetTableManager()->SendDataToTable(m_RoleInfo.dwUserID, pCmd);
}
//void CRoleEx::SaveRoleExInfo()
//{
//	if (!m_IsNeedSave)
//		return;
//	�������������ݱ��浽���ݿ�ȥ
//	DBR_Cmd_SaveRoleExInfo msg;
//	SetMsgInfo(msg,DBR_SaveRoleExInfo, sizeof(DBR_Cmd_SaveRoleExInfo));
//	msg.RoleInfo = m_RoleInfo;
//	msg.RoleCharmValue = g_FishServer.GetFishConfig().GetCharmValue(m_RoleInfo.CharmArray);
//	g_FishServer.SendNetCmdToDB(&msg);
//	m_IsNeedSave = false;
//}

bool CRoleEx::ChangeRoleProduction(DWORD dwProduction)
{
	m_RoleInfo.dwProduction += dwProduction;
	return true;
}

bool CRoleEx::ChangeRoleGameTime(WORD wGameTime)
{
	m_RoleInfo.dwGameTime += wGameTime;
	return true;
}
bool CRoleEx::ChangeRoleTitle(BYTE TitleID)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleTitle");
	//�ı���ҵĳƺ�ID ������ҵĳƺ� ���ƺ�ϵͳ����
	if (m_RoleInfo.TitleID == TitleID)
		return true;
	m_RoleInfo.TitleID = TitleID;
	//1.���͵��ͻ���ȥ 
	LC_Cmd_ChangeRoleTitle msgClient;
	SetMsgInfo(msgClient,GetMsgType(Main_Role, LC_ChangeRoleTitle), sizeof(LC_Cmd_ChangeRoleTitle));
	msgClient.TitleID = TitleID;
	SendDataToClient(&msgClient);
	//2.���͵����������ȥ
	CC_Cmd_ChangeRoleTitle msgCenter;
	SetMsgInfo(msgCenter,GetMsgType(Main_Role, CC_ChangeRoleTitle), sizeof(CC_Cmd_ChangeRoleTitle));
	msgCenter.dwUserID = GetUserID();
	msgCenter.TitleID = TitleID;
	SendDataToCenter(&msgCenter);
	//3.���͵���������ȥ
	LC_Cmd_TableChangeRoleTitle msgTable;
	SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleTitle), sizeof(LC_Cmd_TableChangeRoleTitle));
	msgTable.dwDestUserID = GetUserID();
	msgTable.TitleID = TitleID;
	SendDataToTable(&msgTable);

	DBR_Cmd_SaveRoleCurTitle msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleCurTitle, sizeof(DBR_Cmd_SaveRoleCurTitle));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.TitleID = m_RoleInfo.TitleID;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);


	return true;
}
bool CRoleEx::ChangeRoleAchievementPoint(DWORD dwAchievementPoint)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleAchievementPoint");
	//�޸���ҵĳɾ͵���
	if (dwAchievementPoint == 0)
		return true;
	m_RoleInfo.dwAchievementPoint += dwAchievementPoint;

	LC_Cmd_ChangeRoleAchievementPoint msgClient;
	SetMsgInfo(msgClient,GetMsgType(Main_Role, LC_ChangeRoleAchievementPoint), sizeof(LC_Cmd_ChangeRoleAchievementPoint));
	msgClient.dwAchievementPoint = m_RoleInfo.dwAchievementPoint;
	SendDataToClient(&msgClient);

	LC_Cmd_TableChangeRoleAchievementPoint msgTable;
	SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleAchievementPoint), sizeof(LC_Cmd_TableChangeRoleAchievementPoint));
	msgTable.dwDestUserID = m_RoleInfo.dwUserID;
	msgTable.dwAchievementPoint = m_RoleInfo.dwAchievementPoint;
	SendDataToTable(&msgTable);

	CC_Cmd_ChangeRoleAchievementPoint msgCenter;
	SetMsgInfo(msgCenter,GetMsgType(Main_Role, CC_ChangeRoleAchievementPoint), sizeof(CC_Cmd_ChangeRoleAchievementPoint));
	msgCenter.dwUserID = m_RoleInfo.dwUserID;
	msgCenter.dwAchievementPoint = m_RoleInfo.dwAchievementPoint;
	SendDataToCenter(&msgCenter);

	DBR_Cmd_SaveRoleAchievementPoint msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleAchievementPoint, sizeof(DBR_Cmd_SaveRoleAchievementPoint));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.dwAchievementPoint = m_RoleInfo.dwAchievementPoint;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ChangeRoleCharmValue(BYTE Index, int AddSum)
{
	//g_FishServer.ShowInfoToWin("ChangeRoleCharmValue");
	//�޸���ҵ���������
	if (Index >= MAX_CHARM_ITEMSUM || AddSum == 0)
		return true;
	if (!CheckChangeDWORDValue(m_RoleInfo.CharmArray[Index], AddSum))
		return false;
	m_RoleInfo.CharmArray[Index] += AddSum;
	//1.��������ͻ���ȥ
	LC_Cmd_ChangeRoleCharmValue msgClient;
	SetMsgInfo(msgClient,GetMsgType(Main_Role, LC_ChangeRoleCharmValue), sizeof(LC_Cmd_ChangeRoleCharmValue));
	for (int i = 0; i < MAX_CHARM_ITEMSUM;++i)
		msgClient.CharmInfo[i] = m_RoleInfo.CharmArray[i];
	SendDataToClient(&msgClient);
	//2.����ͬ�����ϵ����
	LC_Cmd_TableChangeRoleCharmValue msgTable;
	SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleCharmValue), sizeof(LC_Cmd_TableChangeRoleCharmValue));
	msgTable.dwDestUserID = m_RoleInfo.dwUserID;
	for (int i = 0; i < MAX_CHARM_ITEMSUM; ++i)
		msgTable.CharmInfo[i] = m_RoleInfo.CharmArray[i];
	SendDataToTable(&msgTable);
	//3.���͵����������ȥ
	CC_Cmd_ChangeRoleCharmValue msgCenter;
	SetMsgInfo(msgCenter,GetMsgType(Main_Role, CC_ChangeRoleCharmValue), sizeof(CC_Cmd_ChangeRoleCharmValue));
	msgCenter.dwUserID = m_RoleInfo.dwUserID;
	for (int i = 0; i < MAX_CHARM_ITEMSUM; ++i)
		msgCenter.CharmInfo[i] = m_RoleInfo.CharmArray[i];
	SendDataToCenter(&msgCenter);

	DBR_Cmd_SaveRoleCharmArray msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleCharmArray, sizeof(DBR_Cmd_SaveRoleCharmArray));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	for (int i = 0; i < MAX_CHARM_ITEMSUM; ++i)
		msgDB.CharmArray[i] = m_RoleInfo.CharmArray[i];
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ChangeRoleSendGiffSum(int AddSum)
{
	//g_FishServer.ShowInfoToWin("ChangeRoleSendGiffSum");
	if (AddSum == 0)
		return true;
	m_RoleInfo.SendGiffSum = ConvertIntToBYTE(m_RoleInfo.SendGiffSum + AddSum);
	//���͵��ͻ���ȥ
	LC_Cmd_ChangeRoleSendGiffSum msg;
	SetMsgInfo(msg,GetMsgType(Main_Role, LC_ChangeRoleSendGiffSum), sizeof(LC_Cmd_ChangeRoleSendGiffSum));
	msg.SendGiffSum = m_RoleInfo.SendGiffSum;
	SendDataToClient(&msg);

	DBR_Cmd_SaveRoleSendGiffSum msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleSendGiffSum, sizeof(DBR_Cmd_SaveRoleSendGiffSum));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.SendSum = m_RoleInfo.SendGiffSum;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ChangeRoleAcceptGiffSum(int AddSum)
{
	//g_FishServer.ShowInfoToWin("ChangeRoleAcceptGiffSum");
	if (AddSum == 0)
		return true;
	m_RoleInfo.AcceptGiffSum = ConvertIntToBYTE(m_RoleInfo.AcceptGiffSum + AddSum);
	LC_Cmd_ChangeRoleAcceptGiffSum msg;
	SetMsgInfo(msg,GetMsgType(Main_Role, LC_ChangeRoleAcceptGiffSum), sizeof(LC_Cmd_ChangeRoleAcceptGiffSum));
	msg.AcceptGiffSum = m_RoleInfo.AcceptGiffSum;
	SendDataToClient(&msg);
	
	DBR_Cmd_SaveRoleAcceptGiffSum msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleAcceptGiffSum, sizeof(DBR_Cmd_SaveRoleAcceptGiffSum));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.AcceptSum = m_RoleInfo.AcceptGiffSum;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ClearRoleTaskStates()
{
	//g_FishServer.ShowInfoToWin("ClearRoleTaskStates");
	int256Handle::Clear(m_RoleInfo.TaskStates);

	DBR_Cmd_SaveRoleTaskStates msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleTaskStates, sizeof(DBR_Cmd_SaveRoleTaskStates));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.States = m_RoleInfo.TaskStates;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ChangeRoleTaskStates(BYTE Index, bool States)
{
	//g_FishServer.ShowInfoToWin("ChangeRoleTaskStates");
	//���������״̬
	if (int256Handle::GetBitStates(m_RoleInfo.TaskStates, Index) == States)
		return true;
	int256Handle::SetBitStates(m_RoleInfo.TaskStates, Index,States);
	//��������ͻ���ȥ
	LC_Cmd_ChangeRoleTaskStates msg;
	SetMsgInfo(msg,GetMsgType(Main_Role, LC_ChangeRoleTaskStates), sizeof(LC_Cmd_ChangeRoleTaskStates));
	msg.Index = Index;
	msg.States = States;
	SendDataToClient(&msg);
	
	DBR_Cmd_SaveRoleTaskStates msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleTaskStates, sizeof(DBR_Cmd_SaveRoleTaskStates));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.States = m_RoleInfo.TaskStates;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ChangeRoleAchievementStates(BYTE Index, bool States)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleAchievementStates");
	if (int256Handle::GetBitStates(m_RoleInfo.AchievementStates, Index) == States)
		return true;
	int256Handle::SetBitStates(m_RoleInfo.AchievementStates, Index, States);
	//��������ͻ���ȥ
	LC_Cmd_ChangeRoleAchievementStates msg;
	SetMsgInfo(msg,GetMsgType(Main_Role, LC_ChangeRoleAchievementStates), sizeof(LC_Cmd_ChangeRoleAchievementStates));
	msg.Index = Index;
	msg.States = States;
	SendDataToClient(&msg);
	
	DBR_Cmd_SaveRoleAchievementStates msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleAchievementStates, sizeof(DBR_Cmd_SaveRoleAchievementStates));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.States = m_RoleInfo.AchievementStates;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ChangeRoleActionStates(BYTE Index, bool States)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleActionStates");
	if (int256Handle::GetBitStates(m_RoleInfo.ActionStates, Index) == States)
		return true;
	int256Handle::SetBitStates(m_RoleInfo.ActionStates, Index, States);
	//��������ͻ���ȥ
	LC_Cmd_ChangeRoleActionStates msg;
	SetMsgInfo(msg,GetMsgType(Main_Role, LC_ChangeRoleActionStates), sizeof(LC_Cmd_ChangeRoleActionStates));
	msg.Index = Index;
	msg.States = States;
	SendDataToClient(&msg);
	
	DBR_Cmd_SaveRoleActionStates msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleActionStates, sizeof(DBR_Cmd_SaveRoleActionStates));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.States = m_RoleInfo.ActionStates;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ChangeRoleOnlineRewardStates(DWORD States)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleOnlineRewardStates");
	if (m_RoleInfo.OnlineRewardStates == States)
		return true;
	m_RoleInfo.OnlineRewardStates = States;

	DBR_Cmd_SaveRoleOnlineStates msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleOnlineStates, sizeof(DBR_Cmd_SaveRoleOnlineStates));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.OnlineStates = m_RoleInfo.OnlineRewardStates;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ChangeRoleCheckData(DWORD CheckData)
{
	//ǩ�����ݰ��¸���

	if (m_RoleInfo.CheckData == CheckData)
		return true;
	m_RoleInfo.CheckData = CheckData;
	//��������ͻ���ȥ
	LC_Cmd_ChangeRoleCheckData msg;
	SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleCheckData), sizeof(LC_Cmd_ChangeRoleCheckData));
	msg.CheckData = CheckData;
	SendDataToClient(&msg);

	DBR_Cmd_SaveRoleCheckData msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleCheckData, sizeof(DBR_Cmd_SaveRoleCheckData));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.CheckData = m_RoleInfo.CheckData;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}
bool CRoleEx::ChangeRoleIsShowIpAddress(bool States)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleIsShowIpAddress");
	//��ҽ����޸� �Ƿ���Ҫ�޸� �Ƿ���ʾIP��ַ
	if (m_RoleInfo.IsShowIPAddress == States)
		return true;
	m_RoleInfo.IsShowIPAddress = States;
	//������͵��ͻ��� �� �������������ȥ ��ҵ�IP��ַ�仯�� �� ���������ȥ
	//1.����Լ� ���߿ͻ��� ������Ա仯�� bool
	LC_Cmd_ChangeRoleIsShowIpAddress msgClient;
	SetMsgInfo(msgClient, GetMsgType(Main_Role, LC_ChangeRoleIsShowIpAddress), sizeof(LC_Cmd_ChangeRoleIsShowIpAddress));
	msgClient.IsShowIpAddress = States;
	SendDataToClient(&msgClient);
	//2.���������
	CC_Cmd_ChangeRoleIsShowIpAddress msgCenter;
	SetMsgInfo(msgCenter, GetMsgType(Main_Role, CC_ChangeRoleIsShowIpAddress), sizeof(CC_Cmd_ChangeRoleIsShowIpAddress));
	msgCenter.IsShowIpAddress = States;
	msgCenter.dwUserID = m_RoleInfo.dwUserID;
	SendDataToCenter(&msgCenter);
	//3.���� ����������������� ��ҵ�IP��ַ�仯��
	LC_Cmd_TableChangeRoleIpAddress msgTable;
	SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleIpAddress), sizeof(LC_Cmd_TableChangeRoleIpAddress));
	msgTable.dwDestUserID = m_RoleInfo.dwUserID;
	if (m_RoleInfo.IsShowIPAddress)
	{
		//չʾ�Լ�������
		TCHARCopy(msgTable.IpAddress, CountArray(msgTable.IpAddress), m_RoleInfo.IPAddress, _tcslen(m_RoleInfo.IPAddress));
	}
	else
	{
		TCHARCopy(msgTable.IpAddress, CountArray(msgTable.IpAddress), Defalue_Ip_Address, _tcslen(Defalue_Ip_Address));
	}
	SendDataToTable(&msgTable);
	//4.���õ�ǰ�������̴洢�����ݿ�ȥ
	
	//OnSaveRoleQueryAtt();

	DBR_Cmd_SaveRoleIsShowIpAddress msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleIsShowIpAddress, sizeof(DBR_Cmd_SaveRoleIsShowIpAddress));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.IsShowIP = m_RoleInfo.IsShowIPAddress;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	return true;
}

// �޸����������Ϣ
bool CRoleEx::ChangeRoleIsOnline(bool States)
{
//	g_FishServer.ShowInfoToWin("���������");
	//��������Ƿ����ߵ�״̬ ��ҽ���AFK ״̬ ���봦�� ֱ�ӽ����ݷ��͵���������� �� ���ݿ�ȥ
	if (m_IsOnline == States)
		return true;
	m_IsOnline = States;

	DBR_Cmd_RoleOnline msg;
	SetMsgInfo(msg, DBR_SetOnline, sizeof(DBR_Cmd_RoleOnline));
	msg.dwUserID = m_RoleInfo.dwUserID;
	msg.IsOnline = States;
	g_FishServer.SendNetCmdToSaveDB(&msg);//��Ҫ���ٱ����

	//���������
	CC_Cmd_ChangeRoleIsOnline msgCenter;	
	SetMsgInfo(msgCenter, GetMsgType(Main_Role, CC_ChangeRoleIsOnline), sizeof(CC_Cmd_ChangeRoleIsOnline));
	msgCenter.dwUserID = m_RoleInfo.dwUserID;
	msgCenter.IsOnline = States;
	SendDataToCenter(&msgCenter);

	//����������������ȥ ���������
	if (!States)
	{
		//g_FishServer.ShowInfoToWin("����������������ȥ���������");
		/*GM_Cmd_LeaveNiuNiuTable msgMini;
		SetMsgInfo(msgMini, GetMsgType(Main_MiniGame, GM_LeaveNiuNiuTable), sizeof(GM_Cmd_LeaveNiuNiuTable));
		msgMini.dwUserID = GetUserID();
		g_FishServer.SendNetCmdToMiniGame(&msgMini);*/

		GM_Cmd_RoleLeaveMiniGame msgLeave;
		SetMsgInfo(msgLeave, GetMsgType(Main_MiniGame, GM_RoleLeaveMiniGame), sizeof(GM_Cmd_RoleLeaveMiniGame));
		msgLeave.dwUserID = GetUserID();
		g_FishServer.SendNetCmdToMiniGame(&msgLeave);

		DBR_Cmd_TableChange msgDB;//��¼��ҽ���
		SetMsgInfo(msgDB, DBR_TableChange, sizeof(DBR_Cmd_TableChange));
		msgDB.dwUserID = GetUserID();
		msgDB.CurrceySum = GetRoleInfo().dwCurrencyNum;
		msgDB.GlobelSum = GetRoleInfo().dwGlobeNum;
		msgDB.MedalSum = GetRoleInfo().dwMedalNum;
		msgDB.JoinOrLeave = false;
		msgDB.LogTime = time(null);
		msgDB.TableTypeID = 250;
		msgDB.TableMonthID = 0;
		g_FishServer.SendNetCmdToSaveDB(&msgDB);
		g_DBLogManager.LogRoleJoinOrLeaveTableInfo(msgDB.dwUserID, msgDB.GlobelSum, msgDB.CurrceySum, msgDB.MedalSum, msgDB.TableTypeID, msgDB.TableMonthID, false, SendLogDB);

		//LogInfoToFile("CollideBuYu.txt", "����������������ȥChangeRoleIsOnline ++++++++++++ %d::::::::::: %d-----------", GetRoleInfo().dwGlobeNum, GetUserID());
	}
	

	if (States)
	{
		string MacAddress = g_FishServer.GetUserMacAddress(GetUserID());
		string IPAddress = g_FishServer.GetUserIpAddress(GetUserID());
		g_DBLogManager.LogRoleOnlineInfo(m_RoleInfo.dwUserID, true, MacAddress, IPAddress, m_RoleInfo.dwGlobeNum, m_RoleInfo.dwCurrencyNum, m_RoleInfo.dwMedalNum, SendLogDB);
	//	g_FishServer.ShowInfoToWin("�������Log��ҽ�ң�%d,�����ʯ��%d,��ҽ���%d",m_RoleInfo.dwGlobeNum, m_RoleInfo.dwCurrencyNum, m_RoleInfo.dwMedalNum);
		//LogInfoToFile("CollideBuYu.txt", "�������Log��ҽ��States+++++++++++++  %d************* %d------", GetRoleInfo().dwGlobeNum, GetUserID());
		//g_FishServer.ShowInfoToWin("��� ID: %d  ������������� ��GameServerIDΪ: %d", UserID, GameID);
	}
	else
	{
		string MacAddress = g_FishServer.GetUserMacAddress(GetUserID());
		string IPAddress = g_FishServer.GetUserIpAddress(GetUserID());
		g_DBLogManager.LogRoleOnlineInfo(m_RoleInfo.dwUserID, false, MacAddress, IPAddress, m_RoleInfo.dwGlobeNum, m_RoleInfo.dwCurrencyNum, m_RoleInfo.dwMedalNum, SendLogDB);
		//g_FishServer.ShowInfoToWin("�������Log");

		//LogInfoToFile("CollideBuYu.txt", "�������Log 1111111111111111//////// %d *****************%d-------", GetRoleInfo().dwGlobeNum, GetUserID());
	//	g_FishServer.ShowInfoToWin("�������Log��ҽ�ң�%d,�����ʯ��%d,��ҽ���%d", m_RoleInfo.dwGlobeNum, m_RoleInfo.dwCurrencyNum, m_RoleInfo.dwMedalNum);

	}
	return true;
}
bool CRoleEx::ChangeRoleExChangeStates(DWORD States)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleExChangeStates�ı�״̬");
	if (m_RoleInfo.ExChangeStates == States)
		return true;
	m_RoleInfo.ExChangeStates = States;
	//��������ͻ���
	LC_Cmd_ChangeRoleExChangeStates msg;
	SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleExChangeStates), sizeof(LC_Cmd_ChangeRoleExChangeStates));
	msg.States = States;
	SendDataToClient(&msg);
	//�����ݿ�
	DBR_Cmd_SaveRoleExChangeStates msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleExChangeStates, sizeof(DBR_Cmd_SaveRoleExChangeStates));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.States = States;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);
	return true;
}
bool CRoleEx::ChangeRoleTotalRechargeSum(DWORD AddSum)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleTotalRechargeSum�޸��ܳ�ֵ������");
	if (AddSum == 0)
		return true;
	m_RoleInfo.TotalRechargeSum += AddSum;//����ܳ�ֵ������ ��λԪ ���Ƿ�

	//���̱��浽���ݿ�ȥ
	DBR_Cmd_SaveRoleTotalRecharge msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleTotalRecharge, sizeof(DBR_Cmd_SaveRoleTotalRecharge));
	msgDB.dwUserID = GetUserID();
	msgDB.Sum = m_RoleInfo.TotalRechargeSum;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);
	//���̷������ͻ���ȥ
	LC_Cmd_ChangeRoleTotalRecharge msg;
	SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleTotalRecharge), sizeof(LC_Cmd_ChangeRoleTotalRecharge));
	msg.Sum = m_RoleInfo.TotalRechargeSum;
	SendDataToClient(&msg);

	GetRoleVip().OnRechargeRMBChange();//����ҳ�ֵ�仯��ʱ�� ���д���
	return true;
}
bool CRoleEx::ChangeRoleIsFirstPayGlobel()
{
//	g_FishServer.ShowInfoToWin("ChangeRoleIsFirstPayGlobel�޸��׳���");
	if (!m_RoleInfo.bIsFirstPayGlobel)
		return false;
	m_RoleInfo.bIsFirstPayGlobel = false;
	//���͵����ݿ�ȥ 
	DBR_Cmd_SaveRoleIsFirstPayGlobel msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleIsFirstPayGlobel, sizeof(DBR_Cmd_SaveRoleIsFirstPayGlobel));
	msgDB.dwUserID = GetUserID();
	g_FishServer.SendNetCmdToSaveDB(&msgDB);
	//���͵��ͻ���ȥ
	LC_Cmd_ChangeRoleIsFirstPayGlobel msg;
	SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleIsFirstPayGlobel), sizeof(LC_Cmd_ChangeRoleIsFirstPayGlobel));
	SendDataToClient(&msg);
	return true;
}
bool CRoleEx::ChangeRoleIsFirstPayCurrcey()
{
//	g_FishServer.ShowInfoToWin("ChangeRoleIsFirstPayCurrcey�޸��׳�");
	if (!m_RoleInfo.bIsFirstPayCurrcey)
		return false;
	m_RoleInfo.bIsFirstPayCurrcey = false;
	//���͵����ݿ�ȥ
	DBR_Cmd_SaveRoleIsFirstPayCurrcey msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleIsFirstPayCurrcey, sizeof(DBR_Cmd_SaveRoleIsFirstPayCurrcey));
	msgDB.dwUserID = GetUserID();
	g_FishServer.SendNetCmdToSaveDB(&msgDB);
	//���͵��ͻ���ȥ
	LC_Cmd_ChangeRoleIsFirstPayCurrcey msg;
	SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleIsFirstPayCurrcey), sizeof(LC_Cmd_ChangeRoleIsFirstPayCurrcey));
	SendDataToClient(&msg);
	return true;
}
bool CRoleEx::ChangeRoleParticularStates(DWORD States)
{
//	g_FishServer.ShowInfoToWin("ChangeRoleParticularStates�޸ļ۸�");
	if (m_RoleInfo.ParticularStates == States)
		return false;
	m_RoleInfo.ParticularStates = States;

	//1.֪ͨ���ݿ�
	DBR_Cmd_SaveRoleParticularStates msg;
	SetMsgInfo(msg, DBR_SaveRoleParticularStates, sizeof(msg));
	msg.dwUserID = m_RoleInfo.dwUserID;
	msg.ParticularStates = m_RoleInfo.ParticularStates;
	g_FishServer.SendNetCmdToSaveDB(&msg);
	//2.֪ͨ��������� 
	CC_Cmd_ChangeRoleParticularStates msgCenter;
	SetMsgInfo(msgCenter, GetMsgType(Main_Role, CC_ChangeRoleParticularStates), sizeof(CC_Cmd_ChangeRoleParticularStates));
	msgCenter.dwUserID = m_RoleInfo.dwUserID;
	msgCenter.ParticularStates = m_RoleInfo.ParticularStates;
	SendDataToCenter(&msgCenter);
	//�ͻ���
	LC_Cmd_ChangeRoleParticularStates msgClient;
	SetMsgInfo(msgClient, GetMsgType(Main_Role, LC_ChangeRoleParticularStates), sizeof(LC_Cmd_ChangeRoleParticularStates));
	msgClient.ParticularStates = m_RoleInfo.ParticularStates;
	SendDataToClient(&msgClient);
	//3.֪ͨminiGame
	GM_Cmd_ChangeRoleParticularStates msgMini;
	SetMsgInfo(msgMini, GetMsgType(Main_MiniGame, GM_ChangeRoleParticularStates), sizeof(GM_Cmd_ChangeRoleParticularStates));
	msgMini.dwUserID = m_RoleInfo.dwUserID;
	msgMini.ParticularStates = m_RoleInfo.ParticularStates;
	g_FishServer.SendNetCmdToMiniGame(&msgMini);
	return true;
}
void CRoleEx::OnRoleCatchFishByLottery(BYTE FishTypeID, CatchType pType, byte subType)
{
	/*if (m_RoleInfo.LotteryFishSum >= g_FishServer.GetFishConfig().GetLotteryConfig().MaxLotteryFishSum)
		return;*/
	HashMap<BYTE, DWORD>::iterator Iter= g_FishServer.GetFishConfig().GetLotteryConfig().FishScore.find(FishTypeID);
	if (Iter == g_FishServer.GetFishConfig().GetLotteryConfig().FishScore.end())
		return;
	else
	{
		CRole* pRole = g_FishServer.GetTableManager()->SearchUser(GetUserID());
		if (pRole)
		{
			if (m_RoleInfo.LotteryFishSum < 0xff)
				m_RoleInfo.LotteryFishSum++;
			WORD RateValue = 1;
			if (pType == CatchType::CATCH_LASER || pType == CatchType::CATCH_BULLET)
				RateValue = g_FishServer.GetTableManager()->GetGameConfig()->BulletMultiple(pRole->GetRateIndex());
			else if (pType == CatchType::CATCH_SKILL)
			{
				RateValue = g_FishServer.GetTableManager()->GetGameConfig()->SkillMultiple(subType);
			}
			m_RoleInfo.LotteryScore += (Iter->second * RateValue);
			m_IsNeedSave = true;
		}
		
	}
}
void CRoleEx::OnClearRoleLotteryInfo()
{
	m_RoleInfo.LotteryFishSum = 0;
	m_RoleInfo.LotteryScore = 0;
	m_IsNeedSave = true;
}
bool CRoleEx::ChangeRoleTotalFishGlobelSum(int AddSum)
{
	//g_FishServer.ShowInfoToWin("ChangeRoleTotalFishGlobelSum�޸Ĳ�����");
	if (AddSum == 0)
		return true;
	m_RoleServerInfo.TotalFishGlobelSum += AddSum;
	m_IsNeedSave = true;
	return true;
}
void CRoleEx::OnChangeRoleSecPassword(DWORD Crc1, DWORD Crc2, DWORD Crc3, bool IsSaveToDB)
{
	if (
		m_RoleServerInfo.SecPasswordCrc1 == Crc1 &&
		m_RoleServerInfo.SecPasswordCrc2 == Crc2 &&
		m_RoleServerInfo.SecPasswordCrc3 == Crc3
		)
		return;

	m_RoleServerInfo.SecPasswordCrc1 = Crc1;
	m_RoleServerInfo.SecPasswordCrc2 = Crc2;
	m_RoleServerInfo.SecPasswordCrc3 = Crc3;
	if (IsSaveToDB)
	{
		DBR_Cmd_SaveRoleSecPassword msg;
		SetMsgInfo(msg, DBR_SaveRoleSecPassword, sizeof(DBR_Cmd_SaveRoleSecPassword));
		msg.dwUserID = GetUserID();
		msg.SecPasswordCrc1 = Crc1;
		msg.SecPasswordCrc2 = Crc2;
		msg.SecPasswordCrc3 = Crc3;
		g_FishServer.SendNetCmdToSaveDB(&msg);
	}
}
//bool CRoleEx::SetRoleMonthCardInfo(BYTE MonthCardID)
//{
//	time_t pNow = time(null);
//	if (MonthCardID == 0)
//	{
//		if (m_RoleInfo.MonthCardID == 0)
//			return true;
//		//�������¿�����
//		m_RoleInfo.MonthCardID = 0;
//		m_RoleInfo.MonthCardEndTime = 0;
//
//		DBR_Cmd_SaveRoleMonthCardInfo msgDB;
//		SetMsgInfo(msgDB, DBR_SaveRoleMonthCardInfo, sizeof(DBR_Cmd_SaveRoleMonthCardInfo));
//		msgDB.UserID = GetUserID();
//		msgDB.MonthCardID = 0;
//		msgDB.MonthCardEndTime = 0;
//		g_FishServer.SendNetCmdToSaveDB(&msgDB);
//		//��������ͻ���
//		LC_Cmd_ChangeRoleMonthCardInfo msg;
//		SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleMonthCardInfo), sizeof(LC_Cmd_ChangeRoleMonthCardInfo));
//		msg.MonthCardID = 0;
//		msg.MonthCardEndTime = 0;
//		SendDataToClient(&msg);
//
//		LC_Cmd_TableChangeRoleIsInMonthCard msgTable;
//		SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleIsInMonthCard), sizeof(LC_Cmd_TableChangeRoleIsInMonthCard));
//		msgTable.dwDestUserID = GetUserID();
//		msgTable.IsInMonthCard = false;
//		SendDataToTable(&msgTable);
//
//		CC_Cmd_ChangeRoleIsInMonthCard msgCenter;
//		SetMsgInfo(msgCenter, GetMsgType(Main_Role, CC_ChangeRoleIsInMonthCard), sizeof(CC_Cmd_ChangeRoleIsInMonthCard));
//		msgCenter.dwUserID = GetUserID();
//		msgCenter.IsInMonthCard = false;
//		SendDataToCenter(&msgCenter);
//
//		return true;
//	}
//	else if (m_RoleInfo.MonthCardID != MonthCardID)
//	{
//		if (m_RoleInfo.MonthCardID != 0 && pNow < m_RoleInfo.MonthCardEndTime)
//		{
//			//��Ҵ����¿�״̬ ���� ��ǰ�¿����� ʹ�õ��¿�
//			return false;
//		}
//		//�滻�µ�ID
//		//�����¿�����Ϣ
//		HashMap<BYTE, tagMonthCardOnce>::iterator Iter = g_FishServer.GetFishConfig().GetMonthCardConfig().MonthCardMap.find(MonthCardID);
//		if (Iter == g_FishServer.GetFishConfig().GetMonthCardConfig().MonthCardMap.end())
//		{
//			ASSERT(false);
//			return false;
//		}
//		//������������¿�������
//		m_RoleInfo.MonthCardID = MonthCardID;
//		m_RoleInfo.MonthCardEndTime = pNow + Iter->second.UseLastMin * 60;//�¿�������ʱ��
//		//GetRoleLauncherManager().OnMonthCardChange(0, MonthCardID);//����µ��¿�
//		//����������ݿ�
//		DBR_Cmd_SaveRoleMonthCardInfo msgDB;
//		SetMsgInfo(msgDB, DBR_SaveRoleMonthCardInfo, sizeof(DBR_Cmd_SaveRoleMonthCardInfo));
//		msgDB.UserID = GetUserID();
//		msgDB.MonthCardID = MonthCardID;
//		msgDB.MonthCardEndTime = m_RoleInfo.MonthCardEndTime;
//		g_FishServer.SendNetCmdToSaveDB(&msgDB);
//		//��������ͻ���
//		LC_Cmd_ChangeRoleMonthCardInfo msg;
//		SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleMonthCardInfo), sizeof(LC_Cmd_ChangeRoleMonthCardInfo));
//		msg.MonthCardID = MonthCardID;
//		msg.MonthCardEndTime = m_RoleInfo.MonthCardEndTime;
//		SendDataToClient(&msg);
//
//		LC_Cmd_TableChangeRoleIsInMonthCard msgTable;
//		SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleIsInMonthCard), sizeof(LC_Cmd_TableChangeRoleIsInMonthCard));
//		msgTable.dwDestUserID = GetUserID();
//		msgTable.IsInMonthCard = true;
//		SendDataToTable(&msgTable);
//
//		CC_Cmd_ChangeRoleIsInMonthCard msgCenter;
//		SetMsgInfo(msgCenter, GetMsgType(Main_Role, CC_ChangeRoleIsInMonthCard), sizeof(CC_Cmd_ChangeRoleIsInMonthCard));
//		msgCenter.dwUserID = GetUserID();
//		msgCenter.IsInMonthCard = true;
//		SendDataToCenter(&msgCenter);
//
//		return true;
//	}
//	else if (m_RoleInfo.MonthCardID == MonthCardID)
//	{
//		//���� 
//		HashMap<BYTE, tagMonthCardOnce>::iterator Iter = g_FishServer.GetFishConfig().GetMonthCardConfig().MonthCardMap.find(MonthCardID);
//		if (Iter == g_FishServer.GetFishConfig().GetMonthCardConfig().MonthCardMap.end())
//		{
//			ASSERT(false);
//			return false;
//		}
//		//������������¿�������
//		m_RoleInfo.MonthCardID = MonthCardID;
//		m_RoleInfo.MonthCardEndTime = max(pNow, m_RoleInfo.MonthCardEndTime) + Iter->second.UseLastMin * 60;//�¿�������ʱ��
//		//GetRoleLauncherManager().OnMonthCardChange(0, MonthCardID);//����µ��¿�
//		//����������ݿ�
//		DBR_Cmd_SaveRoleMonthCardInfo msgDB;
//		SetMsgInfo(msgDB, DBR_SaveRoleMonthCardInfo, sizeof(DBR_Cmd_SaveRoleMonthCardInfo));
//		msgDB.UserID = GetUserID();
//		msgDB.MonthCardID = MonthCardID;
//		msgDB.MonthCardEndTime = m_RoleInfo.MonthCardEndTime;
//		g_FishServer.SendNetCmdToSaveDB(&msgDB);
//		//��������ͻ���
//		LC_Cmd_ChangeRoleMonthCardInfo msg;
//		SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleMonthCardInfo), sizeof(LC_Cmd_ChangeRoleMonthCardInfo));
//		msg.MonthCardID = MonthCardID;
//		msg.MonthCardEndTime = m_RoleInfo.MonthCardEndTime;
//		SendDataToClient(&msg);
//
//		LC_Cmd_TableChangeRoleIsInMonthCard msgTable;
//		SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleIsInMonthCard), sizeof(LC_Cmd_TableChangeRoleIsInMonthCard));
//		msgTable.dwDestUserID = GetUserID();
//		msgTable.IsInMonthCard = true;
//		SendDataToTable(&msgTable);
//
//		CC_Cmd_ChangeRoleIsInMonthCard msgCenter;
//		SetMsgInfo(msgCenter, GetMsgType(Main_Role, CC_ChangeRoleIsInMonthCard), sizeof(CC_Cmd_ChangeRoleIsInMonthCard));
//		msgCenter.dwUserID = GetUserID();
//		msgCenter.IsInMonthCard = true;
//		SendDataToCenter(&msgCenter);
//
//		return true;
//	}
//	else
//	{
//		ASSERT(false);
//		return false;
//	}
//	return false;
//}
//bool CRoleEx::GetRoleMonthCardReward()
//{
//	//�����ͼ��ȡ������¿���Ʒ�Ľ���
//	time_t pNow = time(null);
//	if (m_RoleInfo.MonthCardID == 0 || pNow > m_RoleInfo.MonthCardEndTime)//���¿����� �޷���ȡ�¿���Ʒ
//		return false;
//	if (g_FishServer.GetFishConfig().GetFishUpdateConfig().IsChangeUpdate(m_RoleInfo.GetMonthCardRewardTime, pNow))
//	{
//		HashMap<BYTE, tagMonthCardOnce>::iterator Iter = g_FishServer.GetFishConfig().GetMonthCardConfig().MonthCardMap.find(m_RoleInfo.MonthCardID);
//		if (Iter == g_FishServer.GetFishConfig().GetMonthCardConfig().MonthCardMap.end())
//			return false;
//		WORD RewardID = Iter->second.OnceDayRewardID;
//		OnAddRoleRewardByRewardID(RewardID, TEXT("��ȡ�¿��������"));
//
//		DBR_Cmd_SaveRoleGetMonthCardRewardTime msg;
//		SetMsgInfo(msg, DBR_SaveRoleGetMonthCardRewardTime, sizeof(DBR_Cmd_SaveRoleGetMonthCardRewardTime));
//		msg.UserID = GetUserID();
//		msg.LogTime = pNow;
//		g_FishServer.SendNetCmdToSaveDB(&msg);
//		return true;
//	}
//	else
//	{
//		return false;//�����Ѿ���ȡ���� �޷�����ȡ��
//	}
//}
//bool CRoleEx::ChangeRoleRateValue(BYTE AddRateIndex)
//{
//	//�����ͼ����һ���µı���
//	BYTE UpperRate = min(0, AddRateIndex - 1);
//	if (!int256Handle::GetBitStates(m_RoleInfo.RateValue, UpperRate))
//		return false;
//	if (int256Handle::GetBitStates(m_RoleInfo.RateValue, AddRateIndex))//�Ѿ����������ٴο���
//		return true;
//	//�����µ�
//	int256Handle::SetBitStates(m_RoleInfo.RateValue, AddRateIndex, true);
//	LC_Cmd_ChangeRoleRateValue msg;
//	SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleRateValue), sizeof(LC_Cmd_ChangeRoleRateValue));
//	msg.RateValue = m_RoleInfo.RateValue;
//	SendDataToClient(&msg);
//	//����������ݿ�ȥ
//	DBR_Cmd_SaveRoleRateValue msgDB;
//	SetMsgInfo(msgDB, DBR_SaveRoleRateValue, sizeof(DBR_Cmd_SaveRoleRateValue));
//	msgDB.UserID = GetUserID();
//	msgDB.RateValue = m_RoleInfo.RateValue;
//	g_FishServer.SendNetCmdToSaveDB(&msgDB);
//	return true;
//}
//bool CRoleEx::ChangeRoleVipLevel(BYTE VipLevel, bool IsInit)
//{
//	if (VipLevel == 0)
//	{
//		if (m_RoleInfo.VipLevel == 0)
//			return true;
//	}
//	else
//	{
//		if (VipLevel == m_RoleInfo.VipLevel)
//			return true;
//		HashMap<BYTE, tagVipOnce>::iterator Iter= g_FishServer.GetFishConfig().GetVipConfig().VipMap.find(VipLevel);
//		if (Iter == g_FishServer.GetFishConfig().GetVipConfig().VipMap.end())
//			return false;
//		if (m_RoleInfo.TotalRechargeSum < Iter->second.NeedRechatgeRMBSum)
//			return false;
//	}
//	if (!IsInit)
//		GetRoleLauncherManager().OnVipLevelChange(m_RoleInfo.VipLevel, VipLevel);//��ʼ����ʱ�� �����޸�
//	m_RoleInfo.VipLevel = VipLevel;
//
//	DBR_Cmd_SaveRoleVipLevel msgDB;
//	SetMsgInfo(msgDB, DBR_SaveRoleVipLevel, sizeof(DBR_Cmd_SaveRoleVipLevel));
//	msgDB.VipLevel = m_RoleInfo.VipLevel;
//	msgDB.UserID = GetUserID();
//	g_FishServer.SendNetCmdToSaveDB(&msgDB);
//
//	if (!IsInit)
//	{
//		//����������������������� VIP�ȼ��仯��
//		CC_Cmd_ChangeRoleVipLevel msgCenter;
//		SetMsgInfo(msgCenter, GetMsgType(Main_Role, CC_ChangeRoleVipLevel), sizeof(CC_Cmd_ChangeRoleVipLevel));
//		msgCenter.dwUserID = GetUserID();
//		msgCenter.VipLevel = m_RoleInfo.VipLevel;
//		SendDataToCenter(&msgCenter);
//
//		LC_Cmd_ChangeRoleVipLevel msg;
//		SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleVipLevel), sizeof(LC_Cmd_ChangeRoleVipLevel));
//		msg.VipLevel = m_RoleInfo.VipLevel;
//		SendDataToClient(&msg);
//
//		//����ͬ�����ϵ����
//		LC_Cmd_TableChangeRoleVipLevel msgTable;
//		SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleVipLevel), sizeof(LC_Cmd_TableChangeRoleVipLevel));
//		msgTable.dwDestUserID = GetUserID();
//		msgTable.VipLevel = m_RoleInfo.VipLevel;
//		SendDataToTable(&msgTable);
//	}
//	return true;
//}
//bool CRoleEx::IsCanUseRateIndex(BYTE RateIndex)
//{
//	return int256Handle::GetBitStates(m_RoleInfo.RateValue, RateIndex);//�ж�ָ�������Ƿ����ʹ��
//}
bool CRoleEx::ChangeRoleCashSum(int AddSum)
{
	//g_FishServer.ShowInfoToWin("ChangeRoleCashSum���ݿ�");
	//��Ӷһ�����
	if (AddSum == 0)
		return true;
	if (AddSum < 0 && (m_RoleInfo.CashSum + AddSum) < 0)
		return false;
	m_RoleInfo.CashSum += AddSum;
	if (AddSum > 0)
		m_RoleInfo.TotalCashSum += AddSum;

	//��������ͻ��� �Ѿ� ���ݿ� ���뷢�͵������ ������
	DBR_Cmd_SaveRoleCashSum msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleCashSum, sizeof(DBR_Cmd_SaveRoleCashSum));
	msgDB.UserID = m_RoleInfo.dwUserID;
	msgDB.CashSum = m_RoleInfo.CashSum;
	msgDB.TotalCashSum = m_RoleInfo.TotalCashSum;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	//���͵��ͻ���ȥ
	LC_Cmd_ChangeRoleCashSum msg;
	SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleCashSum), sizeof(LC_Cmd_ChangeRoleCashSum));
	msg.CashSum = m_RoleInfo.CashSum;
	msg.TotalCashSum = m_RoleInfo.TotalCashSum;
	SendDataToClient(&msg);

	return true;
}
bool CRoleEx::ChangeRoleShareStates(bool States)
{
	if (m_RoleInfo.bShareStates == States)
		return false;
	m_RoleInfo.bShareStates = States;

	DBR_Cmd_ChangeRoleShareStates msgDB;
	SetMsgInfo(msgDB, DBR_ChangeRoleShareStates, sizeof(DBR_Cmd_ChangeRoleShareStates));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.States = m_RoleInfo.bShareStates;
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	LC_Cmd_ChangeRoleShareStates msg;
	SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleShareStates), sizeof(LC_Cmd_ChangeRoleShareStates));
	msg.ShareStates = m_RoleInfo.bShareStates;
	SendDataToClient(&msg);

	return true;
}
void CRoleEx::AddRoleProtectSum()
{
	time_t pNow = time(null);
	m_RoleServerInfo.RoleProtectLogTime = pNow;
	m_RoleServerInfo.RoleProtectSum += 1;
	m_IsNeedSave = true;
	{
		m_RoleInfo.benefitCount = m_RoleServerInfo.RoleProtectSum;
		m_RoleInfo.benefitTime = (DWORD)m_RoleServerInfo.RoleProtectLogTime;
	}
}
DWORD CRoleEx::GetScore()
{
	return m_RoleInfo.dwGlobeNum;
}

DWORD CRoleEx::GetExp()
{
	return m_RoleInfo.dwExp;
}
WORD  CRoleEx::GetLevel()
{
	return m_RoleInfo.wLevel;
}
DWORD CRoleEx::GetProduction()
{
	return m_RoleInfo.dwProduction;
}
DWORD CRoleEx::GetGameTime()
{
	return m_RoleInfo.dwGameTime;
}
bool CRoleEx::IsOnceDayOnline()
{
	if (g_FishServer.GetFishConfig().GetFishUpdateConfig().IsChangeUpdate(m_LastOnLineTime, m_LogonTime))
		return false;
	else
		return true;
}
bool CRoleEx::IsOnceMonthOnline()
{
	time_t DestOnLineTime = m_LastOnLineTime - g_FishServer.GetFishConfig().GetWriteSec();
	if (DestOnLineTime < 0)
		DestOnLineTime = 0;
	time_t DestLogonTime = m_LogonTime - g_FishServer.GetFishConfig().GetWriteSec();
	if (DestLogonTime < 0)
		DestLogonTime = 0;
	tm pLogonTime;
	errno_t Error = localtime_s(&pLogonTime, &DestOnLineTime);
	if (Error != 0)
	{
		ASSERT(false);
		return false;
	}

	tm pNowTime;
	Error = localtime_s(&pNowTime, &DestLogonTime);
	if (Error != 0)
	{
		ASSERT(false);
		return false;
	}
	return pNowTime.tm_mon == pLogonTime.tm_mon;
}
void CRoleEx::OnSaveInfoToDB()
{
	//g_FishServer.ShowInfoToWin("OnSaveInfoToDBƽʱ����ͨ�ı���");
	SaveAllRoleInfo(false);//ƽʱ����ͨ�ı��� 15���ӽ���һ�ε�
}
void CRoleEx::UpdateByMin(bool IsHourChange, bool IsDayChange, bool IsMonthChange, bool IsYearChange)
{
	//g_FishServer.ShowInfoToWin("UpdateByMin����Ƿ���Ҫ���б���%d,%d,%d", m_IsAfk, m_IsExit, m_IsRobot);
	//ÿ���Ӹ��´� ����Ƿ���Ҫ���б��� (��ұ���
	if (!m_IsAfk && !m_IsExit && !m_IsRobot)
	{
		//GetaRoleProtect().OnUserNonGlobel();//���ֱ���
	}

	//�ж��¿��Ƿ����
	GetRoleMonthCard().UpdateMonthCard();//�����¿�����

	//�ж����������ӵ� ����ʹ�ü��ܵ�ʱ�� ������ڶ��ٷ��� δ���� ������뿪����
	CRole* pTableRole = g_FishServer.GetTableManager()->SearchUser(GetUserID());
	if (pTableRole && pTableRole->IsNeedLeaveTable())
	{
	//	g_FishServer.ShowInfoToWin("��� ������뿪���� ������뿪����");
		//������뿪����
		g_FishServer.GetTableManager()->OnPlayerLeaveTable(GetUserID());
		//���������ÿͻ�������뿪����
		LC_Cmd_LeaveTableByServer msg;
		SetMsgInfo(msg, GetMsgType(Main_Table, LC_LeaveTableByServer), sizeof(LC_Cmd_LeaveTableByServer));
		msg.IsReturnLogon = false;
		SendDataToClient(&msg);
	}

	//ÿ���� �����������ߵ�ʱ������ 
	if (IsDayChange)
	{
	//	g_FishServer.ShowInfoToWin("��� �����仯 ���и���");
		ChangeRoleSendGiffSum(m_RoleInfo.SendGiffSum * -1);
		ChangeRoleCashSum(m_RoleInfo.CashSum * -1);
		ChangeRoleAcceptGiffSum(m_RoleInfo.AcceptGiffSum * -1);
		ClearRoleTaskStates();
		ChangeRoleOnlineRewardStates(0);

		m_RoleInfo.dwProduction = 0;
		m_RoleInfo.dwGameTime = 0;
		m_RoleInfo.OnlineMin = 0;//���ÿ�����ߵĽ���
		m_LogonTimeByDay = time(NULL);//ÿ������ʱ�� ��¼ʱ�����

		m_IsNeedSave = true;

		m_RoleTask.OnDayChange();	//�������밴����и��� ������� 
		m_MailManager.OnDayChange();//�ʼ�����
		m_RoleGiffManager.OnDayChange();//�������ݽ��и���
		m_RoleRank.UpdateByDay();//������и���
		m_RoleRelationRequest.OnUpdateByDay();

		ResetPerDay();

		//���췢���仯��ʱ��
		SYSTEMTIME time;
		GetLocalTime(&time);
		LC_Cmd_DayChange msg;
		SetMsgInfo(msg,GetMsgType(Main_Role, LC_DayChange), sizeof(LC_Cmd_DayChange));
		msg.Year = (BYTE)(time.wYear - 2000);
		msg.Month = (BYTE)time.wMonth;
		msg.Day = (BYTE)time.wDay;
		msg.Hour = (BYTE)time.wHour;
		msg.Min = (BYTE)time.wMinute;
		msg.Sec = (BYTE)time.wSecond;
		msg.IsDayUpdate = IsDayChange;
		SendDataToClient(&msg);
	}
	if (IsMonthChange)
	{
		//g_FishServer.ShowInfoToWin("��� �·ݱ仯 ���и���");

		ChangeRoleCheckData(0);//���ǩ������
	}
	m_ItemManager.OnUpdateByMin(IsHourChange, IsDayChange, IsMonthChange, IsYearChange);
}
void CRoleEx::OnHandleEvent(bool IsUpdateTask, bool IsUpdateAction, bool IsUpdateAchievement,BYTE EventID, DWORD BindParam, DWORD Param)
{
	if (IsUpdateTask)
		m_RoleTask.OnHandleEvent(EventID, BindParam, Param);
	if (IsUpdateAction)
		m_RoleAchievement.OnHandleEvent(EventID, BindParam, Param);
	if (IsUpdateAchievement)
		m_RoleActionManager.OnHandleEvent(EventID, BindParam, Param);
}
void CRoleEx::OnAddRoleRewardByRewardID(WORD RewardID, const TCHAR* pStr,DWORD RewardSum)
{
	//ֱ�Ӹ�����ҽ���
	HashMap<WORD, tagRewardOnce>::iterator Iter = g_FishServer.GetFishConfig().GetFishRewardConfig().RewardMap.find(RewardID);
	if (Iter == g_FishServer.GetFishConfig().GetFishRewardConfig().RewardMap.end())
		return;
	//��ý���������	
	if (!Iter->second.RewardItemVec.empty())
	{
		vector<tagItemOnce>::iterator IterItem = Iter->second.RewardItemVec.begin();
		for (; IterItem != Iter->second.RewardItemVec.end(); ++IterItem)
		{
			tagItemOnce pOnce = *IterItem;
			pOnce.ItemSum *= RewardSum;
			GetItemManager().OnAddUserItem(pOnce);
		}
	}
	//LogInfoToFile("Control.txt", "ֱ�Ӹ�����ҽ���****************%d*************%d************** ", RewardID, RewardSum);
	//�������ȡ���� ���¼��洢��LOG����ȥ
	g_DBLogManager.LogToDB(GetUserID(), LT_Reward, RewardID, RewardSum, pStr, SendLogDB);//��������
}
void CRoleEx::SetRoleExLeaveServer()
{
	//g_FishServer.ShowInfoToWin("SetRoleExLeaveServer��ҽ�������״̬");
	//1.��ҽ�������״̬
	SetAfkStates(true);
	//2.�������ڷ������� ����Ҵ��е��������
	CRole* pRole = g_FishServer.GetTableManager()->SearchUser(m_RoleInfo.dwUserID);
	if (pRole)
	{
		pRole->SetRoleIsCanSendTableMsg(false);//��������ͻ��˷�������
	}
}

void CRoleEx::ResetRoleInfoToClient()
{
//	g_FishServer.ShowInfoToWin("ResetRoleInfoToClient���ÿͻ�������3333333333");
	ResetClientInfo();//��һЩ������������ˢ�µ�

	LC_Cmd_ResetRoleInfo msg;
	SetMsgInfo(msg,GetMsgType(Main_Role, LC_ResetRoleInfo), sizeof(LC_Cmd_ResetRoleInfo));
	msg.RoleInfo = m_RoleInfo;
	msg.OperateIP = g_FishServer.GetOperateIP();
	SendDataToClient(&msg);

	CRole* pRole= g_FishServer.GetTableManager()->SearchUser(m_RoleInfo.dwUserID);
	if (pRole)
	{
		//�����峡
		NetCmdClearScene cmd;
		cmd.SetCmdSize(sizeof(cmd));
		cmd.SetCmdType(CMD_CLEAR_SCENE);
		cmd.ClearType = 2;
		SendDataToClient(&cmd);

		pRole->SetRoleIsCanSendTableMsg(true);//���Է�������
		//�����������
		pRole->GetChestManager().SendAllChestToClient();//����ǰ��������ı��䷢�͵��ͻ���ȥ
		ServerClientData* pClient = g_FishServer.GetUserClientDataByIndex(GetGameSocketID());
		if (pClient)
		{
			pClient->IsInScene = true;
		}
		g_FishServer.GetTableManager()->ResetTableInfo(m_RoleInfo.dwUserID);
	}
}
void CRoleEx::ResetClientInfo()
{
	//g_FishServer.ShowInfoToWin("ResetClientInfo���ÿͻ�������d");
	m_RelationManager.ResetClientInfo();
	m_ItemManager.ResetClientInfo();
	m_MailManager.ResetClientInfo();
	m_RoleTask.ResetClientInfo();
	m_RoleAchievement.ResetClientInfo();
	m_RoleTitleManager.ResetClientInfo();
	m_RoleIDEntity.ResetClientInfo();
	m_RoleActionManager.ResetClientInfo();
	m_RoleGiffManager.ResetClientInfo();
	//m_RoleGameData.ResetClientInfo();
}
bool CRoleEx::SaveAllRoleInfo(bool IsExit)
{

	//WORD OnLineMin = ConvertDWORDToWORD(GetRoleOnlineSec() / 60);//��ȡ������ߵķ���
	//if (m_RoleInfo.OnlineMin != OnLineMin)
	//{
	//	m_RoleInfo.OnlineMin = OnLineMin;
	//	//m_IsNeedSave = true;
	//	DBR_Cmd_SaveRoleOnlineMin msgDB;
	//	SetMsgInfo(msgDB, DBR_SaveRoleOnlineMin, sizeof(DBR_Cmd_SaveRoleOnlineMin));
	//	msgDB.dwUserID = m_RoleInfo.dwUserID;
	//	msgDB.OnLineMin = m_RoleInfo.OnlineMin;
	//	g_FishServer.SendNetCmdToSaveDB(&msgDB);
	//}
//	g_FishServer.ShowInfoToWin("SaveAllRoleInfoÿ�α����ʱ��,%d", IsExit);
	//ÿ�α����ʱ�� ��������ҵ�����ʱ��
	DBR_Cmd_SaveRoleOnlineMin msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleOnlineMin, sizeof(DBR_Cmd_SaveRoleOnlineMin));
	msgDB.dwUserID = m_RoleInfo.dwUserID;
	msgDB.OnLineMin = ConvertDWORDToWORD(GetRoleOnlineSec() / 60);//������ҵ����߷���
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	if (!m_IsNeedSave && !IsExit)//���뱣��
	{
		SetIsExit(IsExit);
		//m_IsExit = IsExit;
		return false;
	}
	if (IsExit)
	{
		//g_FishServer.ShowInfoToWin("��ҽ������߱���");
	}
	//std::cout << "��� ���ݽ��б���\n";
	//������ҵ�ȫ��������
	SetIsExit(IsExit);
	vector<tagRoleTaskInfo> pTaskVec;
	GetRoleTaskManager().GetAllNeedSaveTask(pTaskVec);

	vector<tagRoleAchievementInfo> pAchievementVec;
	GetRoleAchievementManager().GetAllNeedSaveAchievement(pAchievementVec);

	vector<tagRoleActionInfo> pActionVec;
	GetRoleActionManager().GetAllNeedSaveAction(pActionVec);

	DWORD EventSize = pTaskVec.size() + pAchievementVec.size() + pActionVec.size();
	DWORD PageSize = sizeof(DBR_Cmd_SaveRoleAllInfo)+sizeof(SaveEventInfo)*(EventSize - 1);
	//�������� 10.1
	DBR_Cmd_SaveRoleAllInfo * msg = (DBR_Cmd_SaveRoleAllInfo*)malloc(PageSize);
	if (!msg)
	{
		ASSERT(false);
		return false;
	}
	msg->SetCmdType(DBR_SaveRoleAllInfo);
	//�����������������
	msg->dwUserID = m_RoleInfo.dwUserID;
	msg->dwExp = m_RoleInfo.dwExp;
	msg->dwGlobeNum = m_RoleInfo.dwGlobeNum;//���������Ϣ
	msg->dwProduction = m_RoleInfo.dwProduction;
	msg->dwGameTime = m_RoleInfo.dwGameTime;
	msg->IsNeedResult = IsExit;
	msg->GameData = GetRoleGameData().GetGameData();
	msg->OnlineMin = ConvertDWORDToWORD(GetRoleOnlineSec() / 60);//������ҵ����߷���
	msg->ClientIP = m_RoleInfo.ClientIP;
	msg->dwLotteryScore = m_RoleInfo.LotteryScore;//�齱����
	msg->bLotteryFishSum = m_RoleInfo.LotteryFishSum;//����Ľ����������
	msg->RoleServerInfo = m_RoleServerInfo;//������ҷ��������� ��Щ���Բ����Ϳͻ��˵� Ҳ���뼴�ɱ���

	int i = 0;
	vector<tagRoleTaskInfo>::iterator IterTask = pTaskVec.begin();
	for (; IterTask != pTaskVec.end(); ++IterTask, ++i)
	{
		msg->Array[i].InfoStates = 1;
		msg->Array[i].EventInfo.TaskInfo = *IterTask;
	}
	vector<tagRoleAchievementInfo>::iterator IterAchievement = pAchievementVec.begin();
	for (; IterAchievement != pAchievementVec.end(); ++IterAchievement, ++i)
	{
		msg->Array[i].InfoStates = 2;
		msg->Array[i].EventInfo.AchievementInfo = *IterAchievement;
	}
	vector<tagRoleActionInfo>::iterator IterAction = pActionVec.begin();
	for (; IterAction != pActionVec.end(); ++IterAction, ++i)
	{
		msg->Array[i].InfoStates = 3;
		msg->Array[i].EventInfo.ActionInfo = *IterAction;
	}
	//�ְ�����
	std::vector<DBR_Cmd_SaveRoleAllInfo*> pVec;
	SqlitMsg(msg, PageSize, EventSize,false, pVec);
	free(msg);
	if (!pVec.empty())
	{
		std::vector<DBR_Cmd_SaveRoleAllInfo*>::iterator Iter = pVec.begin();
		for (; Iter != pVec.end(); ++Iter)
		{
			g_FishServer.SendNetCmdToSaveDB(*Iter);//���͵��������ݿ�ȥ ���ձ�������
			free(*Iter);
		}
		pVec.clear();
	}
	//�������Ϻ� 
	m_IsNeedSave = false;


	//LogInfoToFile("LogGold.txt", "SaveAllRoleInfoÿ�α����ʱ��  ::::::::%d:::::::::: %d:::::::", m_RoleInfo.dwGlobeNum, m_RoleInfo.dwUserID);
	return true;
}
void CRoleEx::SetAfkStates(bool States) 
{ 
//	g_FishServer.ShowInfoToWin("��� SetAfkStates��%d,%d", States, m_IsAfk);
	if (m_IsAfk == States)//
		return;
	m_IsAfk = States; 
	ChangeRoleIsOnline(!States); 
}
void CRoleEx::SetIsExit(bool States)
{ 
	//g_FishServer.ShowInfoToWin("��� �˳�SetIsExit��%d,%d", States, m_IsExit);
	if (m_IsExit == States)
		return;
	m_IsExit = States; 
	ChangeRoleIsOnline(!States);
}
void CRoleEx::ResetPerDay()
{
	//g_FishServer.ShowInfoToWin("ResetPerDay����d");
	m_RoleInfo.benefitCount = m_RoleServerInfo.RoleProtectSum = 0;
	m_RoleInfo.benefitTime = 0;
	m_RoleServerInfo.RoleProtectLogTime = 0;
}
void CRoleEx::SendClientOpenShareUI()
{
//	g_FishServer.ShowInfoToWin("SendClientOpenShar����d");
	LC_Cmd_OpenShareUI msg;
	SetMsgInfo(msg, GetMsgType(Main_Role, LC_OpenShareUI), sizeof(LC_Cmd_OpenShareUI));
	SendDataToClient(&msg);
}