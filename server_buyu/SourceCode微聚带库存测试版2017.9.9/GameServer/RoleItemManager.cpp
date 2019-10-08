#include "Stdafx.h" 
#include "RoleItemManager.h"
#include "RoleEx.h"
#include "RoleManager.h"
#include "FishServer.h"
#include "..\CommonFile\DBLogManager.h"
extern void SendLogDB(NetCmd* pCmd);
RoleItemManger::RoleItemManger()
{
	m_IsLoadToClient = false;
	m_IsLoadDB = false;
	m_RoleManager = NULL;
	m_pUser = NULL;
	m_ItemMap.clear();
}
RoleItemManger::~RoleItemManger()
{
	if (!m_ItemMap.empty())
	{
		HashMap<DWORD, tagItemType*>::iterator  Iter = m_ItemMap.begin();
		for (; Iter != m_ItemMap.end(); ++Iter)
		{
			delete Iter->second;
		}
		m_ItemMap.clear();
	}
}
bool RoleItemManger::IsExistsItem(DWORD ItemID)
{
	return g_FishServer.GetFishConfig().ItemIsExists(ItemID);
}
bool RoleItemManger::IsCanUseItem(DWORD ItemID)
{
	if (!IsExistsItem(ItemID))
		return false;
	BYTE ItemTypeID = g_FishServer.GetFishConfig().GetItemType(ItemID);
	if (ItemTypeID == 0)
		return false;
	if (ItemTypeID == EItemType::IT_Package || ItemTypeID == EItemType::IT_MonthCard || ItemTypeID == EItemType::IT_GlobelBag)
		return true;
	else
		return false;
}
bool RoleItemManger::IsCanAcceptItem(DWORD ItemID)
{
	if (!IsExistsItem(ItemID))
		return false;
	BYTE ItemTypeID = g_FishServer.GetFishConfig().GetItemType(ItemID);
	if (ItemTypeID == 0)
		return false;
	if (ItemTypeID == EItemType::IT_GlobelBag)
		return true;
	else
		return false;
}
bool RoleItemManger::OnUseItem(DWORD ItemOnlyID,DWORD ItemID, DWORD ItemSum)
{
	if (!IsCanUseItem(ItemID))
		return false;
	if (!m_pUser)
		return false;
	tagItemConfig* pItemConfig = g_FishServer.GetFishConfig().GetItemInfo(ItemID);
	if (!pItemConfig)
		return false;
	if (pItemConfig->ItemType == EItemType::IT_Package)
	{
		for (size_t i = 0; i < ItemSum; ++i)
			g_FishServer.GetPackageManager().OnOpenFishPackage(m_pUser, ItemOnlyID, ItemID);//�������Ʒ
	}
	else if (pItemConfig->ItemType == EItemType::IT_MonthCard)
	{
		//���ʹ���¿���Ʒ ���ǽ��д���
		if (ItemSum > 1)
		{
			return false;
		}
		if (m_pUser->GetRoleInfo().MonthCardID != 0 && time(null) < m_pUser->GetRoleInfo().MonthCardEndTime)
		{
			return false;
		}
		if (OnDelUserItem(ItemOnlyID, ItemID, ItemSum))
		{
			if (!m_pUser->GetRoleMonthCard().SetRoleMonthCardInfo(ConvertDWORDToBYTE(pItemConfig->ItemParam)))
			{
				tagItemOnce pOnce;
				pOnce.ItemID = ItemID;
				pOnce.ItemSum = ItemSum;
				OnAddUserItem(pOnce);
				ASSERT(false);
				return false;
			}
			return true;
		}
		else
			return false;
	}
	else if (pItemConfig->ItemType == EItemType::IT_GlobelBag)
	{
		//����Ʒ ����Ƿ����������Ʒ
		if (OnDelUserItem(ItemOnlyID, ItemID, ItemSum))
		{
			//��Ʒɾ���ɹ��� ���ǿ�ʼ�������ӽ�Ǯ
			WORD RewardID = static_cast<WORD>(pItemConfig->ItemParam>>16);
			m_pUser->OnAddRoleRewardByRewardID(RewardID, TEXT("��ȡ�۱��轱��"), ItemSum);

			g_DBLogManager.LogToDB(m_pUser->GetUserID(), LT_GlobelBag, static_cast<int>(ItemSum),0, TEXT("ʹ�þ۱���"), SendLogDB);

			return true;
		}
		else
			return false;
	}
	else
	{
		ASSERT(false);
		return false;
	}
	return true;
}
bool RoleItemManger::OnInit(CRoleEx* pUser, RoleManager* pManager)
{
	if (!pUser || !pManager)
	{
		ASSERT(false);
		return false;
	}
	m_RoleManager = pManager;
	m_pUser = pUser;
	return OnLoadUserItem();
}
bool RoleItemManger::OnLoadUserItem()
{
	//�����ݿ������ҵ���Ʒ���� 
	if (!m_pUser)
	{
		ASSERT(false);
		return false;
	}
	m_ItemMap.clear();
	/*DBR_Cmd_LoadUserItem msg;
	SetMsgInfo(msg,DBR_LoadUserItem, sizeof(DBR_Cmd_LoadUserItem));
	msg.dwUserID = m_pUser->GetUserID();
	g_FishServer.SendNetCmdToDB(&msg);*/
	return true;
}
void RoleItemManger::OnLoadUserItemResult(DBO_Cmd_LoadUserItem* pDB)
{
	if (!pDB || !m_pUser)
	{
		ASSERT(false);
		return;
	}
	if ((pDB->States & 1) != 0)
	{
		m_ItemMap.clear();
	}
	for (size_t i = 0; i < pDB->Sum; ++i)
	{
		if (!IsExistsItem(pDB->Array[i].ItemID))
			continue;
		HashMap<DWORD, tagItemType*>::iterator  IterFind = m_ItemMap.find(pDB->Array[i].ItemID);
		if (IterFind == m_ItemMap.end())
		{
			tagItemType* pItem = new tagItemType;
			pItem->OnInit(m_pUser, this);
			pItem->LoadItem(pDB->Array[i]);
			m_ItemMap.insert(HashMap<DWORD, tagItemType*>::value_type(pDB->Array[i].ItemID, pItem));
		}
		else
		{
			IterFind->second->LoadItem(pDB->Array[i]);
		}
	}
	if ((pDB->States & 2) != 0)
	{
		//���е���Ʒ�Ѿ����������
		m_IsLoadDB = true;
		m_pUser->IsLoadFinish();
	}
}
//void RoleItemManger::OnLoadUserItemFinish()
//{
//	if (!m_pUser)
//	{
//		ASSERT(false);
//		return;
//	}
//	
//}
bool RoleItemManger::OnTryAcceptItemToFriend(DWORD DestUserID,DWORD ItemOnlyID, DWORD ItemID, DWORD ItemSum)
{
	if (!m_pUser)
	{
		ASSERT(false);
		return false;
	}
	//������������Ʒ
	HashMap<DWORD, tagItemType*>::iterator Iter = m_ItemMap.find(ItemID);
	if (Iter == m_ItemMap.end())
	{
		ASSERT(false);
		return false;
	}
	if (!IsCanAcceptItem(ItemID))
	{
		ASSERT(false);
		return false;
	}
	tagItemConfig* pItemConfig = g_FishServer.GetFishConfig().GetItemInfo(ItemID);
	if (!pItemConfig)
	{
		ASSERT(false);
		return false;
	}
	//��ǰ��Ʒ�ǿ������͵� 
	/*tagRoleRelation* pRelation = m_pUser->GetRelation().QueryRelationInfo(DestUserID);
	if (!pRelation || pRelation->bRelationType != RT_Friend)
		return false;*/
	if (!OnDelUserItem(ItemOnlyID, ItemID, ItemSum))//�۳���Ʒʧ��
		return false;
	//�����ʼ������
	tagRoleMail	MailInfo;
	MailInfo.bIsRead = false;
	//������������Ҫ����Ĵ��� ������Ҫһ�� �����ת���ַ��� �ͻ��� �� ������ͨ�õ� 
	_sntprintf_s(MailInfo.Context, CountArray(MailInfo.Context), TEXT("%s �������� %u�� {ItemName:ItemID=%u}"), m_pUser->GetRoleInfo().NickName,ItemSum,ItemID);
	//��ItemID ת��Ϊ RewardID �����ʼ�Я�� ��Ʒ���д���? 
	MailInfo.RewardID = static_cast<WORD>(pItemConfig->ItemParam);//�۱���Ľ���ID 
	MailInfo.RewardSum = ItemSum;
	MailInfo.MailID = 0;
	MailInfo.SendTimeLog = time(NULL);
	MailInfo.SrcFaceID = 0;
	TCHARCopy(MailInfo.SrcNickName, CountArray(MailInfo.SrcNickName), TEXT(""), 0);
	MailInfo.SrcUserID = 0;//ϵͳ����
	MailInfo.bIsExistsReward = (MailInfo.RewardID != 0 && MailInfo.RewardSum != 0);
	DBR_Cmd_AddUserMail msg;
	SetMsgInfo(msg, DBR_AddUserMail, sizeof(DBR_Cmd_AddUserMail));
	msg.dwDestUserID = DestUserID;
	msg.MailInfo = MailInfo;
	g_FishServer.SendNetCmdToDB(&msg);

	g_DBLogManager.LogToDB(m_pUser->GetUserID(), LT_GlobelBag, static_cast<int>(ItemSum), DestUserID, TEXT("���;۱���"), SendLogDB);

	return true;
}
bool RoleItemManger::OnTryUseItem(DWORD ItemOnlyID, DWORD ItemID, DWORD ItemSum)
{
	//ʹ�ñ��������Ʒ
	HashMap<DWORD, tagItemType*>::iterator Iter = m_ItemMap.find(ItemID);
	if (Iter == m_ItemMap.end())
	{
		ASSERT(false);
		return false;
	}
	if (!OnUseItem(ItemOnlyID, ItemID, ItemSum)) //ʹ����Ʒ�����Ѿ��۳��� �����ٿ۳�
	{
		ASSERT(false);
		return false;
	}
	return true;
}
bool RoleItemManger::OnGetUserItem()
{
	if (!m_pUser)
	{
		ASSERT(false);
		return false;
	}
	//����ұ�������Ʒ���͵��ͻ���ȥ
	vector<tagItemInfo> pItemList;
	HashMap<DWORD, tagItemType*>::iterator  Iter = m_ItemMap.begin();
	for (; Iter != m_ItemMap.end(); ++Iter)
	{
		tagItemType * pType = Iter->second;
		if (pType->NonTimeItem.ItemID != 0)
		{
			pItemList.push_back(pType->NonTimeItem);
		}

		HashMap<DWORD, tagItemInfo>::iterator IterTimeItem = pType->TimeItem.begin();
		for (; IterTimeItem != pType->TimeItem.end(); ++IterTimeItem)
		{
			pItemList.push_back(IterTimeItem->second);
		}
	}

	DWORD PageSize = sizeof(LC_Cmd_GetUserItem)+sizeof(tagItemInfo)* (pItemList.size() - 1);
	LC_Cmd_GetUserItem* msg = (LC_Cmd_GetUserItem*)malloc(PageSize);
	msg->SetCmdType(GetMsgType(Main_Item, LC_GetUserItem));
	vector<tagItemInfo>::iterator IterItem = pItemList.begin();
	for (WORD i = 0; IterItem != pItemList.end(); ++IterItem, ++i)
	{
		msg->Array[i] = *IterItem;
	}
	std::vector<LC_Cmd_GetUserItem*> pVec;
	SqlitMsg(msg, PageSize, pItemList.size(), true, pVec);
	free(msg);
	if (!pVec.empty())
	{
		std::vector<LC_Cmd_GetUserItem*>::iterator Iter = pVec.begin();
		for (; Iter != pVec.end(); ++Iter)
		{
			m_pUser->SendDataToClient(*Iter);
			free(*Iter);
		}
		pVec.clear();
	}
	m_IsLoadToClient = true;
	return true;
}
bool RoleItemManger::OnAddUserItem(tagItemOnce& pItem)
{
	//�����Ʒ
	if (!m_pUser)
	{
		ASSERT(false);
		return false;
	}
	if (!IsExistsItem(pItem.ItemID))
	{
		return false;
	}
	//����Ӧ�ý��д��� �ж���Ʒ����
	BYTE ItemType = g_FishServer.GetFishConfig().GetItemType(pItem.ItemID);
	//LogInfoToFile("RoleManager.txt", "OnAddUserItem����Ӧ�ý��д��������Ʒ***::::::%d ====::%d*******%d----%d------", pItem.ItemID, ItemType, pItem.ItemSum, m_pUser->GetUserID());

	if (ItemType == 0)
	{
		ASSERT(false);
		return false;
	}
	switch (ItemType)
	{
	case IT_Globel:
		{
			//�����ҽ��
			m_pUser->ChangeRoleGlobe(pItem.ItemSum, true);
			return true;
		}
		break;
	case IT_Medal:
		{
			m_pUser->ChangeRoleMedal(pItem.ItemSum, TEXT("�����������Ʒ ����Ϊ�������� ��ӽ���"));
			return true;
		}
		break;
	case IT_AchievementPoint:
		{
			m_pUser->ChangeRoleAchievementPoint(pItem.ItemSum);
			return true;
		}
		break;
	case IT_Title:
		{
			m_pUser->GetRoleTitleManager().AddRoleTitle(ConvertDWORDToBYTE(pItem.ItemSum));//��Ʒ����Ϊ�ƺ�ID
			return true;
		}
		break;
	case IT_Currey:
		{
			m_pUser->ChangeRoleCurrency(pItem.ItemSum,TEXT("�����������Ʒ ����Ϊ��ʯ���� �����ʯ"));
			return true;
		}
		break;
	case IT_MonthSocre:
		{
			if (m_pUser->GetRoleMonth().IsInMonthTable())
			{
				m_pUser->GetRoleMonth().OnChangeRoleMonthPoint(pItem.ItemSum, true);
			}	  
			return true;
		}
		break;
	case IT_MonthGlobel:
		{
			if (m_pUser->GetRoleMonth().IsInMonthTable())
			{
				m_pUser->GetRoleMonth().OnChangeRoleMonthGlobel(pItem.ItemSum, true);
			}
			return true;
		}
		break;
	case IT_Normal:
	case IT_Skill:
	case IT_Package:
	case IT_Cannon:
	case IT_OsCar:
	case IT_Charm://������Ʒ������ӵ���������ȥ
	case IT_GlobelBag:
	case IT_Horn:
		{
			if (ItemType == IT_GlobelBag)
			{
				g_DBLogManager.LogToDB(m_pUser->GetUserID(), LT_GlobelBag, static_cast<int>(pItem.ItemSum), 0, TEXT("�����������Ʒ ��ƷΪ�۱���"), SendLogDB);
			}
			//��ͨ��Ʒ��ӵ���������ȥ
			HashMap<DWORD, tagItemType*>::iterator  IterFind = m_ItemMap.find(pItem.ItemID);
			if (IterFind != m_ItemMap.end())
			{
				return IterFind->second->AddItem(pItem);
			}
			else
			{
				tagItemType* pNewItemType = new tagItemType;
				pNewItemType->OnInit(m_pUser, this);
				m_ItemMap.insert(HashMap<DWORD, tagItemType*>::value_type(pItem.ItemID, pNewItemType));
				return pNewItemType->AddItem(pItem);
			}
			return true;
		}
		break;
	case IT_MonthCard:
		{
			//������������¿���ʱ�� ֱ��ʹ�õ� ���뱣��
			tagItemConfig* pItemConfig = g_FishServer.GetFishConfig().GetItemInfo(pItem.ItemID);
			if (!pItemConfig)
			{
				ASSERT(false);
				return false;
			}
			BYTE MonthCardID = static_cast<BYTE>(pItemConfig->ItemParam);
			if (!m_pUser->GetRoleMonthCard().SetRoleMonthCardInfo(MonthCardID))
			{
				//ʹ��ʧ�������Ʒ
				HashMap<DWORD, tagItemType*>::iterator  IterFind = m_ItemMap.find(pItem.ItemID);
				if (IterFind != m_ItemMap.end())
				{
					return IterFind->second->AddItem(pItem);
				}
				else
				{
					tagItemType* pNewItemType = new tagItemType;
					pNewItemType->OnInit(m_pUser, this);
					m_ItemMap.insert(HashMap<DWORD, tagItemType*>::value_type(pItem.ItemID, pNewItemType));
					return pNewItemType->AddItem(pItem);
				}
			}
			return true;
		}
		break;
	case IT_Giff:
	case IT_Entity:
		{
		    //������Ʒ����ӵ���������ȥ  
			return false;
		}
	}
	return false;
}
void RoleItemManger::OnAddUserItemResult(DBO_Cmd_AddUserItem* pDB)
{
	if (!pDB)
	{
		ASSERT(false);
		return;
	}
	if (!IsExistsItem(pDB->ItemInfo.ItemID))
	{
		return;
	}
	HashMap<DWORD, tagItemType*>::iterator  IterFind = m_ItemMap.find(pDB->ItemInfo.ItemID);
	if (IterFind == m_ItemMap.end())
	{
		ASSERT(false);
		tagItemType* pNewItemType = new tagItemType;
		pNewItemType->OnInit(m_pUser, this);
		pNewItemType->OnAddItemResult(pDB);
		m_ItemMap.insert(HashMap<DWORD, tagItemType*>::value_type(pDB->ItemInfo.ItemID, pNewItemType));
	}
	else
	{
		IterFind->second->OnAddItemResult(pDB);
	}
}
bool RoleItemManger::OnDelUserItem(DWORD ItemOnlyID, DWORD ItemID, DWORD ItemSum)
{
	if (!m_pUser)
	{
		ASSERT(false);
		return false;
	}
	if (!IsExistsItem(ItemID))
	{
		return false;
	}
	HashMap<DWORD, tagItemType*>::iterator  IterFind = m_ItemMap.find(ItemID);
	if (IterFind == m_ItemMap.end())
	{
		ASSERT(false);
		return false;
	}
	return IterFind->second->DelItem(ItemOnlyID, ItemID, ItemSum);
}
bool RoleItemManger::OnDelUserItem(DWORD ItemID, DWORD ItemSum)
{
	//ɾ�������Ʒ
	if (!m_pUser)
	{
		ASSERT(false);
		return false;
	}
	if (!IsExistsItem(ItemID))
	{
		return false;
	}
	HashMap<DWORD, tagItemType*>::iterator  IterFind = m_ItemMap.find(ItemID);
	if (IterFind == m_ItemMap.end())
	{
		ASSERT(false);
		return false;
	}
	if (ItemSum > IterFind->second->AllItemSum)
	{
		//��Ʒ��������
		HashMap<DWORD, tagItemConvert>::iterator Iter = g_FishServer.GetFishConfig().GetItemConvertConfig().m_ConvertMap.find(ItemID);
		if (Iter == g_FishServer.GetFishConfig().GetItemConvertConfig().m_ConvertMap.end())
			return false;
		DWORD ConvertSum = ItemSum - IterFind->second->AllItemSum;

		if (static_cast<UINT64>(Iter->second.Globel) * ItemSum  > MAXUINT32 ||
			static_cast<UINT64>(Iter->second.Medal) * ItemSum  > MAXUINT32 ||
			static_cast<UINT64>(Iter->second.Currey) * ItemSum  > MAXUINT32
			)
		{
			ASSERT(false);
			return false;
		}

		if (!m_pUser->LostUserMoney(Iter->second.Globel*ConvertSum, Iter->second.Medal*ConvertSum, Iter->second.Currey*ConvertSum,TEXT("�����۳���Ʒ���� ת��Ϊ���Ҵ���")))
			return false;
		if (IterFind->second->DelItem(ItemID, IterFind->second->AllItemSum))//�Ƴ�ȫ������Ʒ
			return true;
		else
		{
			ASSERT(false);
			return true;
		}
	}
	else
	{
		return IterFind->second->DelItem(ItemID, ItemSum);
	}
}
bool RoleItemManger::OnDelUserItemList(vector<tagItemOnce>& pVec)
{
	if (!m_pUser)
	{
		ASSERT(false);
		return false;
	}
	//��Ϊ�漰����Ʒ�����֮�����ֱ��ת���Ĺ��� ������Ҫ����һ���Ե�ɾ�� ����������Ĳ���
	if (pVec.empty())
		return true;
	//������Ҫ��ǰͳ�Ƴ���Ҫ�Ļ������� �Ѿ� ��Ʒ�����Ƿ��㹻
	DWORD Globel = 0, Medal = 0, Currey = 0;
	vector<tagItemOnce>::iterator Iter = pVec.begin();
	for (; Iter != pVec.end(); ++Iter)
	{
		DWORD ItemSum = QueryItemCount(Iter->ItemID);
		if (ItemSum >= Iter->ItemSum)
			continue;
		//��Ʒ���� ���Ҳ�
		HashMap<DWORD, tagItemConvert>::iterator IterConvert = g_FishServer.GetFishConfig().GetItemConvertConfig().m_ConvertMap.find(Iter->ItemID);
		if (IterConvert == g_FishServer.GetFishConfig().GetItemConvertConfig().m_ConvertMap.end())
			return false;
		Globel += (Iter->ItemSum - ItemSum) * IterConvert->second.Globel;
		Medal += (Iter->ItemSum - ItemSum) * IterConvert->second.Medal;
		Currey += (Iter->ItemSum - ItemSum) * IterConvert->second.Currey;
	}
	//�ж���һ����Ƿ��㹻
	if (m_pUser->GetRoleInfo().dwGlobeNum < Globel || m_pUser->GetRoleInfo().dwMedalNum < Medal || m_pUser->GetRoleInfo().dwCurrencyNum < Currey)
	{
		ASSERT(false);
		return false;
	}
	//�۳���Ʒ
	Iter = pVec.begin();
	for (; Iter != pVec.end(); ++Iter)
	{
		if (!OnDelUserItem(Iter->ItemID, Iter->ItemSum))
		{
			ASSERT(false);
		}
	}
	return true;
}
DWORD RoleItemManger::QueryItemCount(DWORD ItemID)
{
	HashMap<DWORD, tagItemType*>::iterator IterFind = m_ItemMap.find(ItemID);
	if (IterFind != m_ItemMap.end())
		return IterFind->second->AllItemSum;
	else
		return 0;
}
DWORD RoleItemManger::QueryItemAllTimeCount(DWORD ItemID)
{
	HashMap<DWORD, tagItemType*>::iterator IterFind = m_ItemMap.find(ItemID);
	if (IterFind != m_ItemMap.end())
		return IterFind->second->NonTimeItem.ItemSum;
	else
		return 0;
}
void RoleItemManger::OnUpdateByMin(bool IsHourChange, bool IsDayChange, bool IsMonthChange, bool IsYearChange)
{
	HashMap<DWORD, tagItemType*>::iterator  Iter = m_ItemMap.begin();
	for (; Iter != m_ItemMap.end(); ++Iter)
	{
		Iter->second->OnUpdateByMin();
	}
}
bool RoleItemManger::GetItemIsAllExists(DWORD ItemID, DWORD ItemSum)
{
	if (ItemSum == 0)
	{
		return true;
	}
	HashMap<DWORD, tagItemType*>::iterator  Iter = m_ItemMap.find(ItemID);//��ȡָ����Ʒ�ļ���
	if (Iter == m_ItemMap.end())
	{
		return false;
	}
	//���� 
	if (Iter->second->AllItemSum < ItemSum)
	{
		return false;
	}
	else
	{
		//��Ʒ�㹻 �ж�
		if (Iter->second->NonTimeItem.ItemSum >= ItemSum)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}

void tagItemType::OnInit(CRoleEx* pRole, RoleItemManger* pManager)
{
	if (!pRole || !pManager)
	{
		ASSERT(false);
		return;
	}
	m_pRole = pRole;
	m_pItemManager = pManager;
}
void tagItemType::LoadItem(tagItemInfo& pInfo)
{
	AllItemSum += pInfo.ItemSum;
	if (pInfo.EndTime == 0)
	{
		if (NonTimeItem.ItemID != 0)
		{
			//ASSERT(false);
			NonTimeItem.ItemSum += pInfo.ItemSum;

			if (pInfo.ItemOnlyID != 0)
			{
				DBR_Cmd_DelUserItem msg;
				SetMsgInfo(msg, DBR_DelUserItem, sizeof(DBR_Cmd_DelUserItem));
				msg.ItemOnlyID = pInfo.ItemOnlyID;
				g_FishServer.SendNetCmdToSaveDB(&msg);
			}

			DBR_Cmd_ChangeUserItem msgChange;
			SetMsgInfo(msgChange,DBR_ChangeUserItem, sizeof(DBR_Cmd_ChangeUserItem));
			msgChange.ItemOnlyID = NonTimeItem.ItemOnlyID;
			msgChange.ItemSum = NonTimeItem.ItemSum;
			g_FishServer.SendNetCmdToSaveDB(&msgChange);
			return;
		}
		NonTimeItem = pInfo;
	}
	else
	{
		TimeItem.insert(HashMap<DWORD, tagItemInfo>::value_type(pInfo.ItemOnlyID, pInfo));
	}
	return;
}
void tagItemType::Destroy()
{
	ZeroMemory(&NonTimeItem, sizeof(NonTimeItem));
	TimeItem.clear();

	AllItemSum = 0;
}
bool tagItemType::AddItem(tagItemOnce& pItem)
{
	if (!m_pRole)
	{
		ASSERT(false);
		return false;
	}
	if (pItem.ItemSum == 0)//��Ʒ����Ϊ0
	{
		return true;
	}


	//�����һ����Ʒ��������ʱ�� �����ж��Ƿ�Ϊ��� �����Ƿ���Ҫ������
	if (g_FishServer.GetPackageManager().IsPackageAndAutoUser(pItem.ItemID) && pItem.ItemSum > 0)
	{
		for (size_t i = 0; i < pItem.ItemSum; ++i)
		{
			//ֱ�Ӵ򿪱���
			g_FishServer.GetPackageManager().OnAutoOpenFishPackage(m_pRole, pItem.ItemID);//Ŀǰ��ұ������޵�ǰ��Ʒ ��Ҫ����ֱ��ת��
		}
		return true;
	}
	if (pItem.LastMin == 0 && NonTimeItem.ItemID != 0)
	{
		NonTimeItem.ItemSum += pItem.ItemSum;
		AllItemSum += pItem.ItemSum;

		

		//���������޸���Ʒ���� change
		DBR_Cmd_ChangeUserItem msg;
		SetMsgInfo(msg,DBR_ChangeUserItem, sizeof(DBR_Cmd_ChangeUserItem));
		msg.ItemOnlyID = NonTimeItem.ItemOnlyID;
		msg.ItemSum = NonTimeItem.ItemSum;
		g_FishServer.SendNetCmdToSaveDB(&msg);

		//���͸ı������ͻ���ȥ
		if (m_pItemManager->IsSendClient())
		{
			LC_Cmd_ChangeUserItem msgClient;
			SetMsgInfo(msgClient,GetMsgType(Main_Item, LC_ChangeUserItem), sizeof(LC_Cmd_ChangeUserItem));
			msgClient.ItemOnlyID = NonTimeItem.ItemOnlyID;
			msgClient.ItemSum = NonTimeItem.ItemSum;
			m_pRole->SendDataToClient(&msgClient);
		}

		m_pRole->GetRoleLauncherManager().OnAddItem(pItem.ItemID);

		return true;
	}
	//����Add  �� DBR �������ݿ�ȥ
	DBR_Cmd_AddUserItem msg;
	SetMsgInfo(msg,DBR_AddUserItem, sizeof(DBR_Cmd_AddUserItem));
	msg.dwUserID = m_pRole->GetUserID();
	msg.ItemInfo = pItem;
	g_FishServer.SendNetCmdToDB(&msg);
	return true;
}
void tagItemType::OnAddItemResult(DBO_Cmd_AddUserItem* pMsg)
{
	if (!pMsg || !m_pRole)
	{
		ASSERT(false);
		return;
	}
	//����DBO  Add ������ ��change����  ���뵽��������ȥ
	AllItemSum += pMsg->ItemInfo.ItemSum;
	if (pMsg->ItemInfo.EndTime == 0)
	{
		if (NonTimeItem.ItemID != 0)
		{
			ASSERT(false);
			AllItemSum -= pMsg->ItemInfo.ItemSum;
			return;
		}
		NonTimeItem = pMsg->ItemInfo;

		

		if (m_pItemManager->IsSendClient())
		{
			LC_Cmd_AddUserItem msg;
			SetMsgInfo(msg,GetMsgType(Main_Item, LC_AddUserItem), sizeof(LC_Cmd_AddUserItem));
			msg.ItemInfo = pMsg->ItemInfo;
			m_pRole->SendDataToClient(&msg);
		}

		m_pRole->GetRoleLauncherManager().OnAddItem(pMsg->ItemInfo.ItemID);
		return;
	}
	else
	{
		TimeItem.insert(HashMap<DWORD, tagItemInfo>::value_type(pMsg->ItemInfo.ItemOnlyID, pMsg->ItemInfo));

		

		if (m_pItemManager->IsSendClient())
		{
			LC_Cmd_AddUserItem msg;
			SetMsgInfo(msg,GetMsgType(Main_Item, LC_AddUserItem), sizeof(LC_Cmd_AddUserItem));
			msg.ItemInfo = pMsg->ItemInfo;
			m_pRole->SendDataToClient(&msg);
		}

		m_pRole->GetRoleLauncherManager().OnAddItem(pMsg->ItemInfo.ItemID);
		return;
	}
}
bool tagItemType::DelItem(DWORD ItemOnlyID, DWORD ItemID, DWORD ItemSum)
{
	if (!m_pRole)
	{
		ASSERT(false);
		return false;
	}
	if (ItemSum == 0)
		return true;
	if (NonTimeItem.ItemOnlyID == ItemOnlyID)
	{
		if (NonTimeItem.ItemSum < ItemSum)
		{
			ASSERT(false);
			return false;
		}
		NonTimeItem.ItemSum -= ItemSum;
		AllItemSum -= ItemSum;
		if (NonTimeItem.ItemSum == 0)
		{
			DBR_Cmd_DelUserItem msg;
			SetMsgInfo(msg, DBR_DelUserItem, sizeof(DBR_Cmd_DelUserItem));
			msg.ItemOnlyID = NonTimeItem.ItemOnlyID;
			g_FishServer.SendNetCmdToSaveDB(&msg);

			if (m_pItemManager->IsSendClient())
			{
				LC_Cmd_DelUserItem msgClient;
				SetMsgInfo(msgClient, GetMsgType(Main_Item, LC_DelUserItem), sizeof(LC_Cmd_DelUserItem));
				msgClient.ItemOnlyID = NonTimeItem.ItemOnlyID;
				m_pRole->SendDataToClient(&msgClient);
			}
			ZeroMemory(&NonTimeItem, sizeof(NonTimeItem));

			m_pRole->GetRoleLauncherManager().OnDelItem(ItemID);
			return true;
		}
		else
		{
			NonTimeItem.ItemSum -= (ItemSum - ItemSum);
			//�����޸ĵ�����
			DBR_Cmd_ChangeUserItem msg;
			SetMsgInfo(msg, DBR_ChangeUserItem, sizeof(DBR_Cmd_ChangeUserItem));
			msg.ItemOnlyID = NonTimeItem.ItemOnlyID;
			msg.ItemSum = NonTimeItem.ItemSum;
			g_FishServer.SendNetCmdToSaveDB(&msg);
			if (m_pItemManager->IsSendClient())
			{
				LC_Cmd_ChangeUserItem msgClient;
				SetMsgInfo(msgClient, GetMsgType(Main_Item, LC_ChangeUserItem), sizeof(LC_Cmd_ChangeUserItem));
				msgClient.ItemOnlyID = NonTimeItem.ItemOnlyID;
				msgClient.ItemSum = NonTimeItem.ItemSum;
				m_pRole->SendDataToClient(&msgClient);
			}

			m_pRole->GetRoleLauncherManager().OnDelItem(ItemID);
			return true;
		}
	}
	else
	{
		HashMap<DWORD, tagItemInfo>::iterator IterItem = TimeItem.find(ItemOnlyID);
		if (IterItem == TimeItem.end())
		{
			ASSERT(false);
			return false;
		}
		if (IterItem->second.ItemSum < ItemSum)
		{
			ASSERT(false);
			return false;
		}
		IterItem->second.ItemSum -= ItemSum;
		AllItemSum -= ItemSum;
		if (IterItem->second.ItemSum == 0)
		{
			DBR_Cmd_DelUserItem msg;
			SetMsgInfo(msg, DBR_DelUserItem, sizeof(DBR_Cmd_DelUserItem));
			msg.ItemOnlyID = ItemOnlyID;
			g_FishServer.SendNetCmdToSaveDB(&msg);
			if (m_pItemManager->IsSendClient())
			{
				LC_Cmd_DelUserItem msgClient;
				SetMsgInfo(msgClient, GetMsgType(Main_Item, LC_DelUserItem), sizeof(LC_Cmd_DelUserItem));
				msgClient.ItemOnlyID = ItemOnlyID;
				m_pRole->SendDataToClient(&msgClient);
			}
			TimeItem.erase(IterItem);
			return true;
		}
		else
		{
			IterItem->second.ItemSum -= ItemSum;
			//����change����
			DBR_Cmd_ChangeUserItem msg;
			SetMsgInfo(msg, DBR_ChangeUserItem, sizeof(DBR_Cmd_ChangeUserItem));
			msg.ItemOnlyID = ItemOnlyID;
			msg.ItemSum = IterItem->second.ItemSum;
			g_FishServer.SendNetCmdToSaveDB(&msg);

			if (m_pItemManager->IsSendClient())
			{
				LC_Cmd_ChangeUserItem msgClient;
				SetMsgInfo(msgClient, GetMsgType(Main_Item, LC_ChangeUserItem), sizeof(LC_Cmd_ChangeUserItem));
				msgClient.ItemOnlyID = ItemOnlyID;
				msgClient.ItemSum = IterItem->second.ItemSum;
				m_pRole->SendDataToClient(&msgClient);
			}
			return true;
		}
	}
}
bool tagItemType::DelItem(DWORD ItemID, DWORD ItemSum)
{
	if (!m_pRole)
	{
		ASSERT(false);
		return false;
	}
	if (ItemSum == 0)
		return true;
	if (ItemSum > AllItemSum)
		return false;
	AllItemSum -= ItemSum;
	DWORD DelItemSum = 0;

	HashMap<DWORD, tagItemInfo>::iterator Iter = TimeItem.begin();
	for (; Iter != TimeItem.end();)
	{
		if (ItemSum - DelItemSum > Iter->second.ItemSum)
		{
			DelItemSum += Iter->second.ItemSum;
			//����ɾ�������� DBR_Del
			DBR_Cmd_DelUserItem msg;
			SetMsgInfo(msg,DBR_DelUserItem, sizeof(DBR_Cmd_DelUserItem));
			msg.ItemOnlyID = Iter->second.ItemOnlyID;
			g_FishServer.SendNetCmdToSaveDB(&msg);
			if (m_pItemManager->IsSendClient())
			{
				LC_Cmd_DelUserItem msgClient;
				SetMsgInfo(msgClient,GetMsgType(Main_Item, LC_DelUserItem), sizeof(LC_Cmd_DelUserItem));
				msgClient.ItemOnlyID = Iter->second.ItemOnlyID;
				m_pRole->SendDataToClient(&msgClient);
			}
			Iter = TimeItem.erase(Iter);
			continue;
		}
		else if (ItemSum - DelItemSum == Iter->second.ItemSum)
		{
			DelItemSum += Iter->second.ItemSum;
			//����ɾ�������� DBR_Del
			DBR_Cmd_DelUserItem msg;
			SetMsgInfo(msg,DBR_DelUserItem, sizeof(DBR_Cmd_DelUserItem));
			msg.ItemOnlyID = Iter->second.ItemOnlyID;
			g_FishServer.SendNetCmdToSaveDB(&msg);
			if (m_pItemManager->IsSendClient())
			{
				LC_Cmd_DelUserItem msgClient;
				SetMsgInfo(msgClient,GetMsgType(Main_Item, LC_DelUserItem), sizeof(LC_Cmd_DelUserItem));
				msgClient.ItemOnlyID = Iter->second.ItemOnlyID;
				m_pRole->SendDataToClient(&msgClient);
			}
			Iter = TimeItem.erase(Iter);
			break;
		}
		else
		{
			Iter->second.ItemSum -= (ItemSum - DelItemSum);
			DelItemSum = ItemSum;
			//����change����
			DBR_Cmd_ChangeUserItem msg;
			SetMsgInfo(msg,DBR_ChangeUserItem, sizeof(DBR_Cmd_ChangeUserItem));
			msg.ItemOnlyID = Iter->second.ItemOnlyID;
			msg.ItemSum = Iter->second.ItemSum;
			g_FishServer.SendNetCmdToSaveDB(&msg);

			if (m_pItemManager->IsSendClient())
			{
				LC_Cmd_ChangeUserItem msgClient;
				SetMsgInfo(msgClient,GetMsgType(Main_Item, LC_ChangeUserItem), sizeof(LC_Cmd_ChangeUserItem));
				msgClient.ItemOnlyID = Iter->second.ItemOnlyID;
				msgClient.ItemSum = Iter->second.ItemSum;
				m_pRole->SendDataToClient(&msgClient);
			}
			break;
		}
	}
	if (DelItemSum < ItemSum)
	{
		if (NonTimeItem.ItemID == 0)
		{
			ASSERT(false);
			return true;
		}
		if (NonTimeItem.ItemSum < ItemSum - DelItemSum)
		{
			ASSERT(false);
			return true;
		}
		else if (NonTimeItem.ItemSum == ItemSum - DelItemSum)
		{
			//����ɾ��������
			DBR_Cmd_DelUserItem msg;
			SetMsgInfo(msg,DBR_DelUserItem, sizeof(DBR_Cmd_DelUserItem));
			msg.ItemOnlyID = NonTimeItem.ItemOnlyID;
			g_FishServer.SendNetCmdToSaveDB(&msg);

			if (m_pItemManager->IsSendClient())
			{
				LC_Cmd_DelUserItem msgClient;
				SetMsgInfo(msgClient,GetMsgType(Main_Item, LC_DelUserItem), sizeof(LC_Cmd_DelUserItem));
				msgClient.ItemOnlyID = NonTimeItem.ItemOnlyID;
				m_pRole->SendDataToClient(&msgClient);
			}
			ZeroMemory(&NonTimeItem, sizeof(NonTimeItem));

			m_pRole->GetRoleLauncherManager().OnDelItem(ItemID);

			return true;
		}
		else
		{
			NonTimeItem.ItemSum -= (ItemSum - DelItemSum);
			//�����޸ĵ�����
			DBR_Cmd_ChangeUserItem msg;
			SetMsgInfo(msg,DBR_ChangeUserItem, sizeof(DBR_Cmd_ChangeUserItem));
			msg.ItemOnlyID = NonTimeItem.ItemOnlyID;
			msg.ItemSum = NonTimeItem.ItemSum;
			g_FishServer.SendNetCmdToSaveDB(&msg);

			if (m_pItemManager->IsSendClient())
			{
				LC_Cmd_ChangeUserItem msgClient;
				SetMsgInfo(msgClient,GetMsgType(Main_Item, LC_ChangeUserItem), sizeof(LC_Cmd_ChangeUserItem));
				msgClient.ItemOnlyID = NonTimeItem.ItemOnlyID;
				msgClient.ItemSum = NonTimeItem.ItemSum;
				m_pRole->SendDataToClient(&msgClient);
			}
			m_pRole->GetRoleLauncherManager().OnDelItem(ItemID);
			return true;
		}
	}
	else
	{
		m_pRole->GetRoleLauncherManager().OnDelItem(ItemID);
		return true;
	}
}
void tagItemType::OnUpdateByMin()
{
	/*if (TimeItem.Size() <= 0)
	return;*/
	if (!m_pRole)
	{
		ASSERT(false);
		return;
	}
	if (TimeItem.empty())
		return;
	time_t NowTime = time(NULL);
	DWORD DelItemSum = 0;
	DWORD ItemID = 0;
	HashMap<DWORD, tagItemInfo>::iterator Iter = TimeItem.begin();
	for (; Iter != TimeItem.end();)
	{
		if (Iter->second.EndTime != 0 && NowTime >= Iter->second.EndTime)
		{
			ItemID = Iter->second.ItemID;
			//ɾ����
			DBR_Cmd_DelUserItem msg;
			SetMsgInfo(msg,DBR_DelUserItem, sizeof(DBR_Cmd_DelUserItem));
			msg.ItemOnlyID = Iter->second.ItemOnlyID;
			g_FishServer.SendNetCmdToSaveDB(&msg);

			//��������ͻ���ȥ ɾ��ָ����Ʒ
			if (m_pItemManager->IsSendClient())
			{
				LC_Cmd_DelUserItem msgClient;
				SetMsgInfo(msgClient,GetMsgType(Main_Item, LC_DelUserItem), sizeof(LC_Cmd_DelUserItem));
				msgClient.ItemOnlyID = Iter->second.ItemOnlyID;
				m_pRole->SendDataToClient(&msgClient);
			}
			DelItemSum += Iter->second.ItemSum;
			Iter = TimeItem.erase(Iter);
		}
		else
			++Iter;
	}
	AllItemSum -= DelItemSum;//��Ʒ��������
	if (DelItemSum > 0)
	{
		m_pRole->GetRoleLauncherManager().OnDelItem(ItemID);//����Ʒ��ɾ����
	}
}