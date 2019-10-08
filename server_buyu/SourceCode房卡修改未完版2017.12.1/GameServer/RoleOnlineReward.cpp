#include "Stdafx.h"
#include "RoleOnlineReward.h"
#include "RoleEx.h"
#include "FishServer.h"
RoleOnlineReward::RoleOnlineReward()
{

}
RoleOnlineReward::~RoleOnlineReward()
{

}
void RoleOnlineReward::OnGetOnlineReward(CRoleEx* pRole,BYTE ID)
{
	LC_Cmd_GetOnlineReward msg;
	SetMsgInfo(msg,GetMsgType(Main_OnlineReward, LC_GetOnlineReward), sizeof(LC_Cmd_GetOnlineReward));
	msg.RewardID = ID;
	if (!pRole)
	{
		ASSERT(false);
		return;
	}
	WORD OnLineMin = ConvertDWORDToWORD(pRole->GetRoleOnlineSec() / 60);
	//msg.OnLineSec = pRole->GetRoleOnlineSec() - (pRole->GetRoleInfo().OnlineMin * 60);//��ȡ�������ߵ�����
	//msg.DBOnLineMin = pRole->GetRoleInfo().OnlineMin;
	HashMap<BYTE, tagOnceOnlienReward>::iterator Iter = g_FishServer.GetFishConfig().GetOnlineRewardConfig().m_OnlineRewardMap.find(ID);
	if (Iter == g_FishServer.GetFishConfig().GetOnlineRewardConfig().m_OnlineRewardMap.end())
	{
		ASSERT(false);
		msg.Result = false;
		pRole->SendDataToClient(&msg);
		return;
	}
	if (pRole->GetRoleInfo().OnlineRewardStates & (1 << (ID - 1)) || Iter->second.OnlineMin > OnLineMin)
	{
		//ASSERT(false);
		msg.Result = false;
		pRole->SendDataToClient(&msg);
		return;
	}
	//��Ҫ��¼����Ƿ���ȡ������ �������������Ƶ������ ������Ҫѭ��������ҵĽ��� 32 DWORD  
	//����ʱ���㹻  ���� δ��ȡ������
	pRole->OnAddRoleRewardByRewardID(Iter->second.RewardID,TEXT("��ȡ���߽���"));
	//2.����������Ϻ����ñ��
	pRole->ChangeRoleOnlineRewardStates(pRole->GetRoleInfo().OnlineRewardStates | (1 << (ID - 1)));
	//3.��������ͻ���ȥ �����ȡ���߽����ɹ���
	msg.Result = true;
	msg.States = pRole->GetRoleInfo().OnlineRewardStates;
	pRole->SendDataToClient(&msg);
	return;
}