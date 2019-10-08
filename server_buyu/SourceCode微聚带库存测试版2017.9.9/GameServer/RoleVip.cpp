#include "Stdafx.h"
#include "RoleVip.h"
#include "RoleEx.h"
#include "FishServer.h"
RoleVip::RoleVip()
{

}
RoleVip::~RoleVip()
{

}
bool RoleVip::OnInit(CRoleEx* pRole)
{
	//�޸����
	//OnRechargeRMBChange();
	//OnChangeRoleVipLevel(5);
	////////////////////////////////////////////////////////////////////////////
	//if (!pRole)
	//{
	//	ASSERT(false);
	//	return false;
	//}
	//m_pRole = pRole;
	//DWORD RechargeValue = m_pRole->GetRoleInfo().TotalRechargeSum;//�ܳ�ֵ����ֵ ������Ҫ��������
	//BYTE VipLevel = 0;
	//HashMap<BYTE, tagVipOnce>::iterator Iter= g_FishServer.GetFishConfig().GetVipConfig().VipMap.begin();
	//for (; Iter != g_FishServer.GetFishConfig().GetVipConfig().VipMap.end(); ++Iter)
	//{
	//	//����ѭ��VIP�ȼ�
	//	if (RechargeValue >= Iter->second.NeedRechatgeRMBSum)
	//		VipLevel = Iter->first;
	//	else
	//		break;
	//}
	//if (m_pRole->GetRoleInfo().VipLevel != VipLevel)
	//{
	//	m_pRole->GetRoleInfo().VipLevel = VipLevel;

	//	DBR_Cmd_SaveRoleVipLevel msgDB;
	//	SetMsgInfo(msgDB, DBR_SaveRoleVipLevel, sizeof(DBR_Cmd_SaveRoleVipLevel));
	//	msgDB.VipLevel = m_pRole->GetRoleInfo().VipLevel;
	//	msgDB.UserID = m_pRole->GetUserID();
	//	g_FishServer.SendNetCmdToSaveDB(&msgDB);
	//}
	//////////////////////////////////////////////////////////////////////////

	if (!pRole)
	{
		ASSERT(false);
		return false;
	}
	m_pRole = pRole;
	DWORD RechargeValue = m_pRole->GetRoleInfo().TotalRechargeSum;//�ܳ�ֵ����ֵ ������Ҫ��������
	BYTE VipLevel = 5;
	
	HashMap<BYTE, tagVipOnce>::iterator Iter = g_FishServer.GetFishConfig().GetVipConfig().VipMap.begin();
	for (; Iter != g_FishServer.GetFishConfig().GetVipConfig().VipMap.end(); ++Iter)
	{
		RechargeValue = Iter->second.NeedRechatgeRMBSum;
		//����ѭ��VIP�ȼ�
		if (RechargeValue >= Iter->second.NeedRechatgeRMBSum)
			VipLevel = Iter->first;
		else
			break;
	}
	if (m_pRole->GetRoleInfo().VipLevel != VipLevel)
	{
		m_pRole->GetRoleInfo().VipLevel = 5;

		DBR_Cmd_SaveRoleVipLevel msgDB;
		SetMsgInfo(msgDB, DBR_SaveRoleVipLevel, sizeof(DBR_Cmd_SaveRoleVipLevel));
		msgDB.VipLevel = m_pRole->GetRoleInfo().VipLevel;
		msgDB.UserID = m_pRole->GetUserID();
		g_FishServer.SendNetCmdToSaveDB(&msgDB);
	}



	return true;
}
void RoleVip::OnRechargeRMBChange()
{
	if (!m_pRole)
	{
		ASSERT(false);
		return;
	}
	DWORD RechargeValue = m_pRole->GetRoleInfo().TotalRechargeSum;//�ܳ�ֵ����ֵ ������Ҫ��������
	BYTE VipLevel = 0;
	HashMap<BYTE, tagVipOnce>::iterator Iter = g_FishServer.GetFishConfig().GetVipConfig().VipMap.begin();
	for (; Iter != g_FishServer.GetFishConfig().GetVipConfig().VipMap.end(); ++Iter)
	{
		//����ѭ��VIP�ȼ�
		if (RechargeValue >= Iter->second.NeedRechatgeRMBSum)
			VipLevel = Iter->first;
		else
			break;
	}

	VipLevel = 5;//vip�ȼ��޸�ԭ�����߼�
	//�ı����VIP�ȼ�
	OnChangeRoleVipLevel(VipLevel);
}
bool RoleVip::OnChangeRoleVipLevel(BYTE VipLevel)
{
	if (VipLevel == 0)
	{
		if (m_pRole->GetRoleInfo().VipLevel == 0)
			return true;
	}
	else
	{
		if (VipLevel == m_pRole->GetRoleInfo().VipLevel)
			return true;
		HashMap<BYTE, tagVipOnce>::iterator Iter = g_FishServer.GetFishConfig().GetVipConfig().VipMap.find(VipLevel);
		if (Iter == g_FishServer.GetFishConfig().GetVipConfig().VipMap.end())
			return false;
		if (m_pRole->GetRoleInfo().TotalRechargeSum < Iter->second.NeedRechatgeRMBSum)
			return false;
	}
	m_pRole->GetRoleLauncherManager().OnVipLevelChange(m_pRole->GetRoleInfo().VipLevel, VipLevel);//��ʼ����ʱ�� �����޸�
	m_pRole->GetRoleInfo().VipLevel = VipLevel;

	DBR_Cmd_SaveRoleVipLevel msgDB;
	SetMsgInfo(msgDB, DBR_SaveRoleVipLevel, sizeof(DBR_Cmd_SaveRoleVipLevel));
	msgDB.VipLevel = 5;//m_pRole->GetRoleInfo().VipLevel;
	msgDB.UserID = m_pRole->GetUserID();
	g_FishServer.SendNetCmdToSaveDB(&msgDB);

	//����������������������� VIP�ȼ��仯��
	CC_Cmd_ChangeRoleVipLevel msgCenter;
	SetMsgInfo(msgCenter, GetMsgType(Main_Role, CC_ChangeRoleVipLevel), sizeof(CC_Cmd_ChangeRoleVipLevel));
	msgCenter.dwUserID = m_pRole->GetUserID();
	msgCenter.VipLevel = 5;//m_pRole->GetRoleInfo().VipLevel;
	m_pRole->SendDataToCenter(&msgCenter);

	LC_Cmd_ChangeRoleVipLevel msg;
	SetMsgInfo(msg, GetMsgType(Main_Role, LC_ChangeRoleVipLevel), sizeof(LC_Cmd_ChangeRoleVipLevel));
	msg.VipLevel = 5;//m_pRole->GetRoleInfo().VipLevel;//vip
	m_pRole->SendDataToClient(&msg);

	//����ͬ�����ϵ����
	LC_Cmd_TableChangeRoleVipLevel msgTable;
	SetMsgInfo(msgTable, GetMsgType(Main_Table, LC_TableChangeRoleVipLevel), sizeof(LC_Cmd_TableChangeRoleVipLevel));
	msgTable.dwDestUserID = m_pRole->GetUserID();
	msgTable.VipLevel = 5;//m_pRole->GetRoleInfo().VipLevel;//vip
	m_pRole->SendDataToTable(&msgTable);

	GM_Cmd_RoleChangeVipLevel msgMini;
	SetMsgInfo(msgMini, GetMsgType(Main_MiniGame, GM_RoleChangeVipLevel), sizeof(GM_Cmd_RoleChangeVipLevel));
	msgMini.dwUserID = m_pRole->GetUserID();
	msgMini.VipLevel = 5;//m_pRole->GetRoleInfo().VipLevel;
	g_FishServer.SendNetCmdToMiniGame(&msgMini);

	return true;
}
BYTE RoleVip::GetLauncherReBoundNum()
{
	if (!m_pRole)
	{
		ASSERT(false);
		return g_FishServer.GetFishConfig().GetVipConfig().DefaultLauncherReBoundNum;
	}
	if (m_pRole->GetRoleInfo().VipLevel == 0)
	{
		return g_FishServer.GetFishConfig().GetVipConfig().DefaultLauncherReBoundNum;
	}
	return g_FishServer.GetFishConfig().GetVipConfig().GetLauncherReBoundNum(m_pRole->GetRoleInfo().VipLevel);
}
//bool  RoleVip::IsCanLauncherLocking()
//{
//	if (!m_pRole)
//	{
//		ASSERT(false);
//		return false;
//	}
//	if (m_pRole->GetRoleInfo().VipLevel == 0)
//	{
//		return false;
//	}
//	return g_FishServer.GetFishConfig().GetVipConfig().IsCanLauncherLocking(m_pRole->GetRoleInfo().VipLevel);
//}
BYTE RoleVip::AddAlmsSum()
{
	if (!m_pRole)
	{
		ASSERT(false);
		return g_FishServer.GetFishConfig().GetVipConfig().DefaultAlmsSum;
	}
	if (m_pRole->GetRoleInfo().VipLevel == 0)
	{
		return g_FishServer.GetFishConfig().GetVipConfig().DefaultAlmsSum;
	}
	return g_FishServer.GetFishConfig().GetVipConfig().AddAlmsSum(m_pRole->GetRoleInfo().VipLevel);
}
float RoleVip::AddMonthScoreRate()
{
	if (!m_pRole)
	{
		ASSERT(false);
		return 1.0f;
	}
	if (m_pRole->GetRoleInfo().VipLevel == 0)
	{
		return 1.0f;
	}
	return g_FishServer.GetFishConfig().GetVipConfig().AddMonthScoreRate(m_pRole->GetRoleInfo().VipLevel);
}
float RoleVip::AddReChargeRate()
{
	if (!m_pRole)
	{
		ASSERT(false);
		return 1.0f;
	}
	if (m_pRole->GetRoleInfo().VipLevel == 0)
	{
		return 1.0f;
	}
	return g_FishServer.GetFishConfig().GetVipConfig().AddReChargeRate(m_pRole->GetRoleInfo().VipLevel);
}
float RoleVip::AddAlmsRate()
{
	if (!m_pRole)
	{
		ASSERT(false);
		return 1.0f;
	}
	if (m_pRole->GetRoleInfo().VipLevel == 0)
	{
		return 1.0f;
	}
	return g_FishServer.GetFishConfig().GetVipConfig().AddAlmsRate(m_pRole->GetRoleInfo().VipLevel);
}
BYTE RoleVip::GetUseMedalSum()
{
	if (!m_pRole)
	{
		ASSERT(false);
		return  g_FishServer.GetFishConfig().GetVipConfig().DefaultUseMedalSum;
	}
	if (m_pRole->GetRoleInfo().VipLevel == 0)
	{
		return  g_FishServer.GetFishConfig().GetVipConfig().DefaultUseMedalSum;
	}
	return g_FishServer.GetFishConfig().GetVipConfig().GetUseMedalSum(m_pRole->GetRoleInfo().VipLevel);
}
float RoleVip::AddCatchFishRate()
{
	if (!m_pRole)
	{
		ASSERT(false);
		return 1.0f;
	}
	if (m_pRole->GetRoleInfo().VipLevel == 0)
	{
		return 1.0f;
	}
	return g_FishServer.GetFishConfig().GetVipConfig().AddCatchFishRate(m_pRole->GetRoleInfo().VipLevel);
}