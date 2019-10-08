#include "StdAfx.h"
#include "Role.h"
#include "TableManager.h"
#include "FishLogic\FishScene\FishResManager.h"
#include "FishServer.h"
#include<atlconv.h>
#include <mmsystem.h>

#include "..\CommonFile\FishServerConfig.h"
#include<algorithm>

#pragma comment(lib, "winmm.lib")
#include "..\CommonFile\DBLogManager.h"

extern void SendLogDB(NetCmd* pCmd);

static volatile long	g_UpdateCount = 0;
static volatile bool	g_bRun = true;
struct TableUpdateParam
{
	GameTable* pTable;
	bool m_bUpdateTime;
	bool m_bUpdateRp;
	bool m_bUpdateRpEffect;
};
SafeList<TableUpdateParam*> g_SafeList[THREAD_NUM];
UINT WINAPI TableThreadUpdate(void *p)
{
	int idx = (int)p;
	SafeList<TableUpdateParam*> &safeList = g_SafeList[idx];
	while (g_bRun)
	{
		TableUpdateParam* pDesk = safeList.GetItem();
		while (pDesk)
		{
			//���ݲ�����������
			if (!pDesk)
				continue;
			if (!pDesk->pTable)
			{
				free(pDesk);
				continue;
			}
			pDesk->pTable->Update(pDesk->m_bUpdateTime, pDesk->m_bUpdateRp, pDesk->m_bUpdateRpEffect);
			::InterlockedIncrement(&g_UpdateCount);
			free(pDesk);

			pDesk = safeList.GetItem();
		}
		Sleep(1);
	}
	return 0;
}



//ȫ�����ӹ�����
TableManager::TableManager() //:m_GameConfig(this)
{
	m_pGameConfig = new CConfig(this);
	m_MaxTableID = 0;

	m_TableTypeSum.clear();
	m_TableGlobeMap.clear();

	RNumrateF = 0;
	RNumrateS = 0;
	RNumrateT = 0;
	RNumrateR = 0;
	
	//KucunNum[]
//	memset(KucunNum, 0, sizeof(KucunNum));
	ZeroMemory(KucunNum, sizeof(KucunNum));

	//��ʼ��˽������ID��
	for (unsigned short i = 0; i < 65535; ++i)
		m_privateTableIDPool.push_back(i);



}
TableManager::~TableManager()
{
	Destroy();
}
void TableManager::OnInit()
{
	//��ȡȫ�ֵ������ļ�
	FishCallback::SetFishSetting(m_pGameConfig);
	if (!FishResManager::Inst() || !FishResManager::Inst()->Init(L"fishdata"))
	{
		//AfxMessageBox(TEXT("��ȡfishdataʧ�ܣ�����"));
		return;
	}

	{
		//USES_CONVERSION;
		//if (!m_GameConfig.LoadConfig(W2A(L"fishdata")))
		//{
		//	//AfxMessageBox(TEXT("��ȡgame configʧ��"));
		//	return;
		//}
		//m_TimerRanomCatch.StartTimer(m_pGameConfig->RandomCatchCycle()*MINUTE2SECOND, REPEATTIMER);//?
		m_TimerRanomCatch.StartTimer(m_pGameConfig->RandomCatchCycle(), REPEATTIMER);//?
		m_TimerRp.StartTimer(m_pGameConfig->RpCycle(), REPEATTIMER);
		m_TimerGameTime.StartTimer(GAME_TIME_SPACE, REPEATTIMER);
	}
	
	for (int i = 0; i < THREAD_NUM; ++i)//�����߳�
	{
		::_beginthreadex(0, 0, (TableThreadUpdate), (void*)i, 0, 0);
	}
	m_LastUpdate = timeGetTime();
}
//ɾ������   ������Ҫ
void TableManager::Destroy()
{
	//�����ٵ�ʱ�� 
	//1.���������Ӷ���ȫ�����ٵ� 
	g_bRun = false;
	if (!m_TableVec.empty())
	{
		std::vector<GameTable*>::iterator Iter = m_TableVec.begin();
		for (; Iter != m_TableVec.end(); ++Iter)
		{
			if (!(*Iter))
				continue;
			//CTraceService::TraceString(TEXT("һ�����Ӷ��� �Ѿ�������"), TraceLevel_Normal);
			delete *Iter;
		}
		m_TableVec.clear();
	}
	//2.����ȫ���������ļ�
	if (FishResManager::Inst())
		FishResManager::Inst()->Shutdown();
}
void TableManager::OnStopService()
{
	//��������ֹͣ��ʱ��
	Destroy();//���ӹ�����ֹͣ��������ʱ�� ���ǹرս���
}
void TableManager::Update(DWORD dwTimeStep)
{
	if (m_TableVec.empty())
		return;
	float TimeStep = (dwTimeStep - m_LastUpdate) * 0.001f;
	m_LastUpdate = dwTimeStep;

	if (m_TimerRanomCatch.Update(TimeStep))
	{

		//m_pGameConfig->RandomdFishByTime();
	}

	bool m_bUpdateTime = m_TimerGameTime.Update(TimeStep);
	bool m_bUpdateRp = m_TimerRp.Update(TimeStep);
	bool m_bUpdateRpEffect = m_TimerRpAdd.Update(TimeStep);// zhi hou
	if (m_bUpdateRp)
	{
		m_TimerRpAdd.StartTimer(m_pGameConfig->RpEffectCycle(), 1);
	}
	//������������и���
	g_UpdateCount = 0;
	int UpdateSum = 0;
	for (UINT i = 0; i < m_TableVec.size(); ++i)
	{
		if (!m_TableVec[i])
			continue;
		if (m_TableVec[i]->GetTablePlayerSum() == 0)
			continue;
		int idx = i % THREAD_NUM;
		TableUpdateParam* pParam = (TableUpdateParam*)malloc(sizeof(TableUpdateParam));
		if (!pParam)
		{
			ASSERT(false);
			return;
		}
		pParam->m_bUpdateRp = m_bUpdateRp;
		pParam->m_bUpdateTime = m_bUpdateTime;
		pParam->m_bUpdateRpEffect = m_bUpdateRpEffect;
		pParam->pTable = m_TableVec[i];
		g_SafeList[idx].AddItem(pParam);
		++UpdateSum;
	}
	while (g_UpdateCount != UpdateSum)
		Sleep(1);
}
GameTable* TableManager::GetTable(WORD TableID)
{
	{
	if (TableID == 0xffff || TableID >= m_TableVec.size())
		//ASSERT(false);
		return NULL;
	}
	GameTable* pTable = m_TableVec[TableID];
	return pTable;
}
bool TableManager::OnPlayerJoinTable(WORD TableID, CRoleEx* pRoleEx, bool IsSendToClient)
{
	//��ҽ���һ������ 
	//�ж�����Ƿ���Լ��뷿��
	//���ȷ�������Ƿ���Խ��뷿������� ����ɹ��� �ڷ���������ҵ�����
	if (!pRoleEx)
	{
		ASSERT(false);
		return false;
	}
	GameTable* pTable = GetTable(TableID);
	if (!pTable)
	{
		ASSERT(false);
		return false;
	}
	BYTE TableTypeID = pTable->GetTableTypeID();
	HashMap<DWORD, WORD>::iterator Iter = m_RoleTableMap.find(pRoleEx->GetUserID());
	if (Iter != m_RoleTableMap.end())
	{
		//����Ѿ����������޷�������������
		LC_JoinTableResult msg;
		SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		ASSERT(false);
		return false;
	}
	HashMap<BYTE, TableInfo>::iterator IterConfig = g_FishServer.GetFishConfig().GetTableConfig().m_TableConfig.find(TableTypeID);
	if (IterConfig == g_FishServer.GetFishConfig().GetTableConfig().m_TableConfig.end())
	{
		LC_JoinTableResult msg;
		SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		ASSERT(false);
		return false;
	}

	if (pRoleEx->IsRobot() && !pTable->IsCanAddRobot())
	{
		return false;
	}
	if (pTable->OnRoleJoinTable(pRoleEx, pTable->GetTableMonthID(), IsSendToClient))
	{
		ServerClientData* pData = g_FishServer.GetUserClientDataByIndex(pRoleEx->GetGameSocketID());
		if (pData)
		{
			pData->IsInScene = true;
		}

		if (pTable->GetTableMonthID() == 0)
			OnChangeTableTypePlayerSum(TableTypeID, true);

		

		DBR_Cmd_TableChange msgDB;//��¼��ҽ���
		SetMsgInfo(msgDB, DBR_TableChange, sizeof(DBR_Cmd_TableChange));
		msgDB.dwUserID = pRoleEx->GetUserID();
		msgDB.CurrceySum = pRoleEx->GetRoleInfo().dwCurrencyNum;
		msgDB.GlobelSum = pRoleEx->GetRoleInfo().dwGlobeNum;
		msgDB.MedalSum = pRoleEx->GetRoleInfo().dwMedalNum;
		msgDB.JoinOrLeave = true;
		msgDB.LogTime = time(null);
		msgDB.TableTypeID = TableTypeID;
		msgDB.TableMonthID = pTable->GetTableMonthID();
		g_FishServer.SendNetCmdToSaveDB(&msgDB);
		g_DBLogManager.LogRoleJoinOrLeaveTableInfo(msgDB.dwUserID, msgDB.GlobelSum, msgDB.CurrceySum, msgDB.MedalSum, msgDB.TableTypeID, msgDB.TableMonthID, true, SendLogDB);

		//�û��ɹ��������� ������Ҫ��������ͻ���ȥ ���߿ͻ�����ҽ�����Ϸ�ɹ�
		m_RoleTableMap.insert(HashMap<DWORD, WORD>::value_type(pRoleEx->GetUserID(), pTable->GetTableID()));
		LogInfoToFile("CollideBuYu.txt", "OnPlayerJoinTable������������ 33322 *******-----%d******************** %d-----", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());


		if (pTable->GetTableMonthID() == 0 && !pRoleEx->IsRobot())
			g_FishServer.GetRobotManager().OnRoleJoinNormalRoom(pTable);
		return true;
	}
	else
	{
		//����������� �����Խ������� ������������ж�
		//��������ͻ��� ������ҽ�������ʧ����
		LC_JoinTableResult msg;
		SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		return false;
	}
}


//��������
void TableManager::OnPlayerSitDown(BYTE TableTypeID, CRoleEx* pRoleEx, NetCmd* pData, BYTE MonthID, bool IsSendToClient)
{
	if (!pRoleEx)
	{
		//ASSERT(false);
		return;
	}

	//��ҽ�������
	CL_JoinTable* pMsg = (CL_JoinTable*)pData;
	if (!pMsg)
	{
		return;
	}
	LogInfoToFile("CollideBuYu.txt", "��������  ��������  ��������  ��������Ӵ���������Ϣ*****%d********** %d-----", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());
	////////////////////////////////////////////////////////////////////////////	
	//std::shared_ptr<CTableFrame> pTableFrame;

	//������Լ�������˽�з��䣬��ص��Լ��ķ��䡣
	auto iter = m_UserID_2_PriTableID.find(pRoleEx->GetUserID());
	if (iter != m_UserID_2_PriTableID.end())
	{
		int sPrivateTableID = iter->second;
		//�ҵ�����
		GameTable* pTable = GetTable(m_PriTableID_2_TableID[sPrivateTableID]);
		//pTableFrame = GetPriTableFrame(sPrivateTableID);
		//������Ӵ���
		if (pTable)
		{
			//1��ʾ����
			if (1 == pMsg->bCreateTable)
			{
				//��ͨ��Ҳ����ظ��������䣬vip�û������ظ��������䡣
				//if (0 == pIServerUserItem->GetMemberOrder())		
				//{	//��vip��������
				//	SendRequestFailure(pIServerUserItem, TEXT("���Ѵ����˷��䣬���Ƚ�ɢ��ǰ���䣡"), 0);
				//	return true;
				//}
				LogInfoToFile("CollideBuYu.txt", TEXT("���������Լ�������˽�з���1"));
				return ;
			}
			else
			{
				//////////////////////////////////////////////////////////////////////////�޸�ԭ�߼�  �������
				//��VIP����Ĭ�Ϸ���0����λ��VIP������Ҫ���������λ
				//if (pTable->bVipTable())
				//	wRequestChairID = pTableFrame->GetNullChairID();
				//else
				//	//wRequestChairID = 0;//����Ĭ����0��λ��GetNullChairIDFree
				//	wRequestChairID = pTableFrame->GetNullChairIDFree();
				//	//wRequestChairID = pTableFrame->GetNullChairID();
				//pTable->PerformSitDownAction(wRequestChairID, pIServerUserItem, pUserSitDown->szPassword, 1);//���뷿��
				//////////////////////////////////////////////////////////////////////////

				LogInfoToFile("CollideBuYu.txt", TEXT("���������Լ�������˽�з���2"));
				DWORD NowTime = timeGetTime();//���������ʱ��

				//���Ӳ���� ���ǿ�ʼ����
				if (pTable->OnRoleJoinTable(pRoleEx, MonthID, IsSendToClient))
				{
					//��ҽ������ӳɹ���
					ServerClientData* pData = g_FishServer.GetUserClientDataByIndex(pRoleEx->GetGameSocketID());
					if (pData)
					{
						pData->IsInScene = true;
					}

					if (MonthID == 0)
						OnChangeTableTypePlayerSum(TableTypeID, true);

					DBR_Cmd_TableChange msgDB;//��¼��ҽ���
					SetMsgInfo(msgDB, DBR_TableChange, sizeof(DBR_Cmd_TableChange));
					msgDB.dwUserID = pRoleEx->GetUserID();
					msgDB.CurrceySum = pRoleEx->GetRoleInfo().dwCurrencyNum;
					msgDB.GlobelSum = pRoleEx->GetRoleInfo().dwGlobeNum;
					msgDB.MedalSum = pRoleEx->GetRoleInfo().dwMedalNum;
					msgDB.JoinOrLeave = true;
					msgDB.LogTime = time(null);
					msgDB.TableTypeID = TableTypeID;
					msgDB.TableMonthID = MonthID;
					g_FishServer.SendNetCmdToSaveDB(&msgDB);
					g_DBLogManager.LogRoleJoinOrLeaveTableInfo(msgDB.dwUserID, msgDB.GlobelSum, msgDB.CurrceySum, msgDB.MedalSum, msgDB.TableTypeID, msgDB.TableMonthID, true, SendLogDB);
					m_RoleTableMap.insert(HashMap<DWORD, WORD>::value_type(pRoleEx->GetUserID(), m_TableVec.size() - 1));
					LogInfoToFile("CollideBuYu.txt", "OnPlayerJoinTable�������� ����33322 ******%d********** %d-----", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());
					
					if (MonthID == 0 && !pRoleEx->IsRobot())
						g_FishServer.GetRobotManager().OnRoleCreateNormalRoom(pTable);

					return;
				}
				else
				{
					LC_JoinTableResult msg;
					SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
					msg.bTableTypeID = TableTypeID;
					msg.Result = false;
					pRoleEx->SendDataToClient(&msg);
					ASSERT(false);
					return;
				}
				return ;
				////////////////////////////////////////////////////////////////////////
			}

		}
		else {

			//���Ӳ����ڣ�Ȼ�󴴽�
			m_UserID_2_PriTableID.erase(iter);
			LogInfoToFile("CollideBuYu.txt", TEXT("���������Լ�������˽�з���3"));
			////֪ͨɾ��˽�з����
			//CMD_GR_USER_PRIVATE_TABLE_ID req;
			//lstrcpy(req.szPrivateTableID, L"");
			//req.dwUserID = 0;
			//SendData(pIServerUserItem, MDM_GR_USER, SUB_GR_USER_PRIVATE_TABLE_ID, &req, sizeof(req));

			//if (2 == pUserSitDown->cbCreateTable)
			//{
			//	SendRequestFailure(pIServerUserItem, TEXT("�Բ����㻹û�д������䣡"), 0);
			//	return true;
			//}
			return;
		}

	}


	if (0 != pMsg->bCreateTable)	//����˽������
	{
		LogInfoToFile("CollideBuYu.txt", TEXT("���������Լ�������˽�з���4"));
		//////////////////////////////////////////////////////////////////////////
		//tagPrivateRoomCost cost;
		//ZeroMemory(&cost, sizeof(cost));
		if (1 == pMsg->bCreateTable) {

			//tagGameConfig* pConfig = (tagGameConfig*)pUserSitDown->TableConfig;
			//auto pTableFrame = GetTableFrameInstance();
			////���ﴴ������ 
			//if (!pTableFrame)
			//{
			//	//SendRequestFailure(pIServerUserItem, TEXT("û��������䣬��ȷ�Ϻ����ԣ�11"), 0);
			//	return;
			//}
			LogInfoToFile("CollideBuYu.txt", "����˽������****%d********** %d-----", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());
			GameTable* pTable = new GameTable();
			if (!pTable)
			{
				LC_JoinTableResult msg;
				SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
				msg.bTableTypeID = TableTypeID;
				msg.Result = false;
				pRoleEx->SendDataToClient(&msg);
				//ASSERT(false);
				return;
			}
			auto tableId = m_privateTableIDPool.front();
			m_privateTableIDPool.pop_front();

			pTable->OnInit(tableId, TableTypeID, MonthID);//�����ӳ�ʼ��   ����������
			//m_MaxTableID++;
			if (pTable)
			{
				auto tableId = pTable->GetTableID();
				//	m_TableFrameArray[tableId] = table;
				pTable->GetPrivateTableID() = pMsg->szPrivateRoomID;//����Ӧ�ô����ݿⷵ�ط����
				pTable->GetCreateTime() = time(0);
			}
			//�������õ����ӷ�������
			m_UserID_2_PriTableID[pRoleEx->GetUserID()] = pTable->GetPrivateTableID();
			m_PriTableID_2_TableID[pTable->GetPrivateTableID()] = pTable->GetTableID();
			m_TableVec.push_back(pTable);
			//���÷���
			pTable->SetPrivateTableOwnerID(pRoleEx->GetUserID());

			DWORD NowTime = timeGetTime();//���������ʱ��
			//pTableFrame->GetConsumeCurrency(cost, pConfig, 1);
		}
		else if (2 == pMsg->bCreateTable)
		{
			//����Ǽ��뷿��
			///	pTableFrame = GetPriTableFrame(pUserSitDown->szPrivateTableID);
			LogInfoToFile("CollideBuYu.txt", TEXT("���������Լ�������˽�з���5"));			
			GameTable* pTable = GetTable(m_PriTableID_2_TableID[pMsg->szPrivateRoomID]);
			if (!pTable)
			{
				//SendRequestFailure(pIServerUserItem, TEXT("û��������䣬��ȷ�Ϻ����ԣ�"), 0);
				return;
			}
			DWORD NowTime = timeGetTime();//���뷿���ʱ��
			// 			pTableFrame->GetConsumeCurrency(cost);
			// 			tagGameConfig* pConfig = (tagGameConfig*)pUserSitDown->TableConfig;	// 
			// 			if (pConfig->wPayCosType == 1)//����֧��
			// 			{
			// 				cost.dwFeeCount = 0;
			// 			}

			LogInfoToFile("CollideBuYu.txt", "����Ǽ��뷿��****%d********** %d-----", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());
		}
	}

	//�ҵ���ѯ������ ���ǽ�����תΪDBR���� ���͵����ݿ�ȥ
	DBR_Cmd_QueryPrivateRoom msg;
	SetMsgInfo(msg, DBR_QueryPrivateRoom, sizeof(DBR_Cmd_QueryPrivateRoom));
	msg.cbCreateRoom = (1 == pMsg->bCreateTable) ? 1 : 0;
	//if (pConfig->wPayCosType==1&& pIServerUserItem->GetUserID()!=)//����֧��
	//{
	//	req.dwCostFeeCount = cost.dwFeeCount;
	//}

	msg.dwCostFeeCount = 1;//���������    dang��Ȼ���ֻ�����ݼӼ�  ��Ҫͬ���ͻ��˲���   ����Ļ���
	msg.wFeeType = 1;// cost.wFeeType;   //0 ���  1��ʯ
	msg.dwUserID = pRoleEx->GetUserID();
	msg.wDataSize = sizeof(DBR_Cmd_QueryPrivateRoom);
	//��Ӳ���
	//DWORD NowTime = timeGetTime();//���������ʱ��

	msg.wDifen = 1;//pConfig->wDifen;
	msg.wPlayCountRule = 10;//pConfig->wPlayCountRule;//ԭ���Ǿ���������ʱ��   ʱ������  10fen
	msg.bTableTypeID = pMsg->bTableTypeID;
	g_FishServer.SendNetCmdToDB(&msg);
	LogInfoToFile("CollideBuYu.txt", TEXT("���������Լ�������˽�з���46"));

	return;
}
//ɾ������  ���� 
bool TableManager::DeletePrivateTable(GameTable* pTableFrame, CRoleEx* pRoleEx)
{
	if (!pTableFrame)
		return false;
	if (!pRoleEx)
	{
		return false;
	}
	//std::vector<GameTable*>::iterator Iter = m_TableVec.begin();

	m_PriTableID_2_TableID.erase(pTableFrame->GetPrivateTableID());
	m_UserID_2_PriTableID.erase(pTableFrame->GetPrivateTableOwnerID());
	m_privateTableIDPool.push_back(pTableFrame->GetTableID());

	DBR_GR_DeletePrivateRoom req;	
	SetMsgInfo(req, DBR_DeletePrivateRoom, sizeof(DBR_GR_DeletePrivateRoom));
	req.dwUserID = pTableFrame->GetPrivateTableOwnerID();
//	lstrcpy(req.szPrivateTableID, pTableFrame->GetPrivateTableID().c_str());
	req.szPrivateTableID = pTableFrame->GetPrivateTableID();
	g_FishServer.SendNetCmdToDB(&req);
	//m_pIRecordDataBaseEngine->PostDataBaseRequest(DBR_GR_DELETE_PRIVATE_ROOM, 0, &req, sizeof(req));
	return true;
}

//����ר��
void TableManager::OnPlayerCreateOrJoinTable(BYTE TableTypeID, CRoleEx* pRoleEx, NetCmd* pData, BYTE MonthID, bool IsSendToClient)
{
	LogInfoToFile("CollideBuYu.txt", TEXT("����ר��--------"));
	if (!pRoleEx)
	{		
		return;
	}
	//��ҽ�������
	CL_JoinTableOne* pMsg = (CL_JoinTableOne*)pData;

	if (!pMsg)
	{		
		return ;
	}

	LogInfoToFile("CollideBuYu.txt", TEXT("����ר��"));
	HashMap<DWORD, WORD>::iterator Iter = m_RoleTableMap.find(pRoleEx->GetUserID());
	if (Iter != m_RoleTableMap.end())
	{
		//����Ѿ����������޷�������������
		LC_JoinTableResult msg;
		SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		ASSERT(false);
		return;
	}
	LogInfoToFile("CollideBuYu.txt", TEXT("����ר��1"));
	HashMap<BYTE, TableInfo>::iterator IterConfig = g_FishServer.GetFishConfig().GetTableConfig().m_TableConfig.find(TableTypeID);
	if (IterConfig == g_FishServer.GetFishConfig().GetTableConfig().m_TableConfig.end())
	{
		LC_JoinTableResult msg;
		SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		ASSERT(false);
		return;
	}
	LogInfoToFile("CollideBuYu.txt", TEXT("����ר��2"));
	// ���
	if (pMsg->bCreateTable==2)
	{
		GameTable* pTable = GetTable(m_PriTableID_2_TableID[pMsg->szPrivateRoomID]);
		if (!pTable)
		{
			//SendRequestFailure(pIServerUserItem, TEXT("û��������䣬��ȷ�Ϻ����ԣ�"), 0);
			return;
		}

		LogInfoToFile("CollideBuYu.txt", TEXT("����ר��3"));
		//���Ӳ���� ���ǿ�ʼ����
		if (pTable->OnRoleJoinTable(pRoleEx, MonthID, IsSendToClient))
		{
			//��ҽ������ӳɹ���
			ServerClientData* pData = g_FishServer.GetUserClientDataByIndex(pRoleEx->GetGameSocketID());
			if (pData)
			{
				pData->IsInScene = true;
			}

			if (MonthID == 0)
				OnChangeTableTypePlayerSum(TableTypeID, true);

			DBR_Cmd_TableChange msgDB;//��¼��ҽ���
			SetMsgInfo(msgDB, DBR_TableChange, sizeof(DBR_Cmd_TableChange));
			msgDB.dwUserID = pRoleEx->GetUserID();
			msgDB.CurrceySum = pRoleEx->GetRoleInfo().dwCurrencyNum;
			msgDB.GlobelSum = pRoleEx->GetRoleInfo().dwGlobeNum;
			msgDB.MedalSum = pRoleEx->GetRoleInfo().dwMedalNum;
			msgDB.JoinOrLeave = true;
			msgDB.LogTime = time(null);
			msgDB.TableTypeID = TableTypeID;
			msgDB.TableMonthID = MonthID;
			g_FishServer.SendNetCmdToSaveDB(&msgDB);
			g_DBLogManager.LogRoleJoinOrLeaveTableInfo(msgDB.dwUserID, msgDB.GlobelSum, msgDB.CurrceySum, msgDB.MedalSum, msgDB.TableTypeID, msgDB.TableMonthID, true, SendLogDB);

			m_RoleTableMap.insert(HashMap<DWORD, WORD>::value_type(pRoleEx->GetUserID(), m_TableVec.size() - 1));
			LogInfoToFile("CollideBuYu.txt", "���� ���� ��¼��ҽ���33322 ******%d********** %d-----", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());


			if (MonthID == 0 && !pRoleEx->IsRobot())
				g_FishServer.GetRobotManager().OnRoleCreateNormalRoom(pTable);

			return;
		}
		else
		{
			LC_JoinTableResult msg;
			SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
			msg.bTableTypeID = TableTypeID;
			msg.Result = false;
			pRoleEx->SendDataToClient(&msg);
			ASSERT(false);
			return;
		}

		//////////////////////////////////////////////////////////////////////////
// 		std::vector<GameTable*>::iterator IterVec = m_TableVec.begin();
// 		//�Զ������Ѵ��ڵ�����  �����Щ���Ӷ������˾ʹ���һ��������  
// 		for (WORD i = 0; IterVec != m_TableVec.end(); ++IterVec, ++i)
// 		{
// 			if (!(*IterVec))
// 				continue;
// 
// 			if ((*IterVec)->GetTableTypeID() == TableTypeID && MonthID == (*IterVec)->GetTableMonthID() && !(*IterVec)->IsFull())
// 			{
// 				//�жϻ������Ƿ���Լ��뵱ǰ����
// 				if (pRoleEx->IsRobot() && !(*IterVec)->IsCanAddRobot())
// 				{
// 					continue;
// 				}
// 				if ((*IterVec)->OnRoleJoinTable(pRoleEx, MonthID, IsSendToClient))
// 				{
// 					ServerClientData* pData = g_FishServer.GetUserClientDataByIndex(pRoleEx->GetGameSocketID());
// 					if (pData)
// 					{
// 						pData->IsInScene = true;
// 					}
// 
// 					if (MonthID == 0)
// 						OnChangeTableTypePlayerSum(TableTypeID, true);
// 
// 					DBR_Cmd_TableChange msgDB;//��¼��ҽ���
// 					SetMsgInfo(msgDB, DBR_TableChange, sizeof(DBR_Cmd_TableChange));
// 					msgDB.dwUserID = pRoleEx->GetUserID();
// 					msgDB.CurrceySum = pRoleEx->GetRoleInfo().dwCurrencyNum;
// 					msgDB.GlobelSum = pRoleEx->GetRoleInfo().dwGlobeNum;
// 					msgDB.MedalSum = pRoleEx->GetRoleInfo().dwMedalNum;
// 					msgDB.JoinOrLeave = true;
// 					msgDB.LogTime = time(null);
// 					msgDB.TableTypeID = TableTypeID;
// 					msgDB.TableMonthID = MonthID;
// 					g_FishServer.SendNetCmdToSaveDB(&msgDB);
// 					g_DBLogManager.LogRoleJoinOrLeaveTableInfo(msgDB.dwUserID, msgDB.GlobelSum, msgDB.CurrceySum, msgDB.MedalSum, msgDB.TableTypeID, msgDB.TableMonthID, true, SendLogDB);
// 
// 					//�û��ɹ��������� ������Ҫ��������ͻ���ȥ ���߿ͻ�����ҽ�����Ϸ�ɹ�
// 					m_RoleTableMap.insert(HashMap<DWORD, WORD>::value_type(pRoleEx->GetUserID(), i));
// 
// 					LogInfoToFile("CollideBuYu.txt", "OnPlayerJoinTable��ҽ���һ������ 33322 ******%d --------%d------", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());
// 
// 					if (MonthID == 0 && !pRoleEx->IsRobot())
// 						g_FishServer.GetRobotManager().OnRoleJoinNormalRoom(*IterVec);
// 
// 					return;
// 				}
// 				else
// 				{
// 					//����������� �����Խ������� ������������ж�
// 					//��������ͻ��� ������ҽ�������ʧ����
// 					LC_JoinTableResult msg;
// 					SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
// 					msg.bTableTypeID = TableTypeID;
// 					msg.Result = false;
// 					pRoleEx->SendDataToClient(&msg);
// 					return;
// 				}
// 			}
		//}

	}
else if (pMsg->bCreateTable == 1)//����
{

	LogInfoToFile("CollideBuYu.txt", "���� ���� ******%d********** %d-----", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());
	GameTable* pTable = new GameTable();
	if (!pTable)
	{
		LC_JoinTableResult msg;
		SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		//ASSERT(false);
		return;
	}
	auto tableId = m_privateTableIDPool.front();
	m_privateTableIDPool.pop_front();

	pTable->OnInit(tableId, TableTypeID, MonthID);//�����ӳ�ʼ��   ����������
	//m_MaxTableID++;
	if (pTable)
	{
		auto tableId = pTable->GetTableID();
	//	m_TableFrameArray[tableId] = table;

		pTable->GetPrivateTableID() = pMsg->szPrivateRoomID;//����Ӧ�ô����ݿⷵ�ط����
		pTable->GetCreateTime() = time(0);
	}

	LogInfoToFile("CollideBuYu.txt", TEXT("����ר��55"));
	
	//�������õ����ӷ�������
	m_UserID_2_PriTableID[pRoleEx->GetUserID()] = pTable->GetPrivateTableID();
	m_PriTableID_2_TableID[pTable->GetPrivateTableID()] = pTable->GetTableID();

	m_TableVec.push_back(pTable);

	//���Ӳ���� ���ǿ�ʼ����
	if (pTable->OnRoleJoinTable(pRoleEx, MonthID, IsSendToClient))
	{
		//��ҽ������ӳɹ���
		ServerClientData* pData = g_FishServer.GetUserClientDataByIndex(pRoleEx->GetGameSocketID());
		if (pData)
		{
			pData->IsInScene = true;
		}

		if (MonthID == 0)
			OnChangeTableTypePlayerSum(TableTypeID, true);

		DBR_Cmd_TableChange msgDB;//��¼��ҽ���
		SetMsgInfo(msgDB, DBR_TableChange, sizeof(DBR_Cmd_TableChange));
		msgDB.dwUserID = pRoleEx->GetUserID();
		msgDB.CurrceySum = pRoleEx->GetRoleInfo().dwCurrencyNum;
		msgDB.GlobelSum = pRoleEx->GetRoleInfo().dwGlobeNum;
		msgDB.MedalSum = pRoleEx->GetRoleInfo().dwMedalNum;
		msgDB.JoinOrLeave = true;
		msgDB.LogTime = time(null);
		msgDB.TableTypeID = TableTypeID;
		msgDB.TableMonthID = MonthID;
		g_FishServer.SendNetCmdToSaveDB(&msgDB);
		g_DBLogManager.LogRoleJoinOrLeaveTableInfo(msgDB.dwUserID, msgDB.GlobelSum, msgDB.CurrceySum, msgDB.MedalSum, msgDB.TableTypeID, msgDB.TableMonthID, true, SendLogDB);

		m_RoleTableMap.insert(HashMap<DWORD, WORD>::value_type(pRoleEx->GetUserID(), m_TableVec.size() - 1));
		LogInfoToFile("CollideBuYu.txt", "OnPlayerJoinTable���Ӳ���� 33322 ******%d********** %d-----", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());


		if (MonthID == 0 && !pRoleEx->IsRobot())
			g_FishServer.GetRobotManager().OnRoleCreateNormalRoom(pTable);

		return;
	}
	else
	{
		LogInfoToFile("CollideBuYu.txt", TEXT("����ר��222"));
		LC_JoinTableResult msg;
		SetMsgInfo(msg, GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		ASSERT(false);
		return;
	}

}



return;


}
//����ģʽ�޸����   ���ӵ�����
void TableManager::OnPlayerJoinTable(BYTE TableTypeID, CRoleEx* pRoleEx, BYTE MonthID, bool IsSendToClient)
{
	//��ҽ���һ������ 
	//�ж�����Ƿ���Լ��뷿��
	//���ȷ�������Ƿ���Խ��뷿������� ����ɹ��� �ڷ���������ҵ�����
	if (!pRoleEx)
	{
		ASSERT(false);
		return;
	}
	HashMap<DWORD, WORD>::iterator Iter = m_RoleTableMap.find(pRoleEx->GetUserID());
	if (Iter != m_RoleTableMap.end())
	{
		//����Ѿ����������޷�������������
		LC_JoinTableResult msg;
		SetMsgInfo(msg,GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		ASSERT(false);
		return;
	}

	HashMap<BYTE, TableInfo>::iterator IterConfig =  g_FishServer.GetFishConfig().GetTableConfig().m_TableConfig.find(TableTypeID);
	if (IterConfig == g_FishServer.GetFishConfig().GetTableConfig().m_TableConfig.end())
	{
		LC_JoinTableResult msg;
		SetMsgInfo(msg,GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		ASSERT(false);
		return;
	}
	std::vector<GameTable*>::iterator IterVec = m_TableVec.begin();
	//�Զ������Ѵ��ڵ�����  �����Щ���Ӷ������˾ʹ���һ��������  
	for (WORD i = 0; IterVec != m_TableVec.end(); ++IterVec, ++i)
	{
		if (!(*IterVec))
			continue;

		if ((*IterVec)->GetTableTypeID() == TableTypeID && MonthID == (*IterVec)->GetTableMonthID() && !(*IterVec)->IsFull())
		{
			//�жϻ������Ƿ���Լ��뵱ǰ����
			if (pRoleEx->IsRobot() && !(*IterVec)->IsCanAddRobot())
			{
				continue;
			}
			if ((*IterVec)->OnRoleJoinTable(pRoleEx,MonthID,IsSendToClient))
			{
				ServerClientData* pData = g_FishServer.GetUserClientDataByIndex(pRoleEx->GetGameSocketID());
				if (pData)
				{
					pData->IsInScene = true;
				}

				if (MonthID == 0)
					OnChangeTableTypePlayerSum(TableTypeID, true);

				DBR_Cmd_TableChange msgDB;//��¼��ҽ���
				SetMsgInfo(msgDB, DBR_TableChange, sizeof(DBR_Cmd_TableChange));
				msgDB.dwUserID = pRoleEx->GetUserID();
				msgDB.CurrceySum = pRoleEx->GetRoleInfo().dwCurrencyNum;
				msgDB.GlobelSum = pRoleEx->GetRoleInfo().dwGlobeNum;
				msgDB.MedalSum = pRoleEx->GetRoleInfo().dwMedalNum;
				msgDB.JoinOrLeave = true;
				msgDB.LogTime = time(null);
				msgDB.TableTypeID = TableTypeID;
				msgDB.TableMonthID = MonthID;
				g_FishServer.SendNetCmdToSaveDB(&msgDB);
				g_DBLogManager.LogRoleJoinOrLeaveTableInfo(msgDB.dwUserID, msgDB.GlobelSum, msgDB.CurrceySum, msgDB.MedalSum, msgDB.TableTypeID, msgDB.TableMonthID, true, SendLogDB);

				//�û��ɹ��������� ������Ҫ��������ͻ���ȥ ���߿ͻ�����ҽ�����Ϸ�ɹ�
				m_RoleTableMap.insert(HashMap<DWORD, WORD>::value_type(pRoleEx->GetUserID(), i));

				LogInfoToFile("CollideBuYu.txt", "OnPlayerJoinTable��ҽ���һ������ 33322 ******%d --------%d------", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());

				if (MonthID == 0 && !pRoleEx->IsRobot())
					g_FishServer.GetRobotManager().OnRoleJoinNormalRoom(*IterVec);

				return;
			}
			else
			{
				//����������� �����Խ������� ������������ж�
				//��������ͻ��� ������ҽ�������ʧ����
				LC_JoinTableResult msg;
				SetMsgInfo(msg,GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
				msg.bTableTypeID = TableTypeID;
				msg.Result = false;
				pRoleEx->SendDataToClient(&msg);
				return;
			}
		}
	}
	GameTable* pTable = new GameTable();
	if (!pTable)
	{
		LC_JoinTableResult msg;
		SetMsgInfo(msg,GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		//ASSERT(false);
		return;
	}
	pTable->OnInit(m_MaxTableID, TableTypeID,MonthID);//�����ӳ�ʼ��   ����������
	m_MaxTableID++;
	m_TableVec.push_back(pTable);
	//���Ӳ���� ���ǿ�ʼ����
	if (pTable->OnRoleJoinTable(pRoleEx,MonthID,IsSendToClient))
	{
		//��ҽ������ӳɹ���
		ServerClientData* pData = g_FishServer.GetUserClientDataByIndex(pRoleEx->GetGameSocketID());
		if (pData)
		{
			pData->IsInScene = true;
		}

		if (MonthID == 0)
			OnChangeTableTypePlayerSum(TableTypeID, true);

		DBR_Cmd_TableChange msgDB;//��¼��ҽ���
		SetMsgInfo(msgDB, DBR_TableChange, sizeof(DBR_Cmd_TableChange));
		msgDB.dwUserID = pRoleEx->GetUserID();
		msgDB.CurrceySum = pRoleEx->GetRoleInfo().dwCurrencyNum;
		msgDB.GlobelSum = pRoleEx->GetRoleInfo().dwGlobeNum;
		msgDB.MedalSum = pRoleEx->GetRoleInfo().dwMedalNum;
		msgDB.JoinOrLeave = true;
		msgDB.LogTime = time(null);
		msgDB.TableTypeID = TableTypeID;
		msgDB.TableMonthID = MonthID;
		g_FishServer.SendNetCmdToSaveDB(&msgDB);
		g_DBLogManager.LogRoleJoinOrLeaveTableInfo(msgDB.dwUserID, msgDB.GlobelSum, msgDB.CurrceySum, msgDB.MedalSum, msgDB.TableTypeID, msgDB.TableMonthID, true, SendLogDB);

		m_RoleTableMap.insert(HashMap<DWORD, WORD>::value_type(pRoleEx->GetUserID(), m_TableVec.size() - 1));
		LogInfoToFile("CollideBuYu.txt", "OnPlayerJoinTable���Ӳ���� 33322 ******%d********** %d-----", pRoleEx->GetRoleInfo().dwGlobeNum, pRoleEx->GetUserID());


		if (MonthID == 0 && !pRoleEx->IsRobot())
			g_FishServer.GetRobotManager().OnRoleCreateNormalRoom(pTable);

		return;
	}
	else
	{
		LC_JoinTableResult msg;
		SetMsgInfo(msg,GetMsgType(Main_Table, LC_Sub_JoinTable), sizeof(LC_JoinTableResult));
		msg.bTableTypeID = TableTypeID;
		msg.Result = false;
		pRoleEx->SendDataToClient(&msg);
		ASSERT(false);
		return;
	}
}
void TableManager::ResetTableInfo(DWORD dwUserID)
{
	HashMap<DWORD, WORD>::iterator Iter = m_RoleTableMap.find(dwUserID);
	if (Iter == m_RoleTableMap.end())
	{
		//ASSERT(false);
		return;
	}
	WORD Index = Iter->second;
	if (m_TableVec.size() <= Index)
	{
		ASSERT(false);
	}
	GameTable* pTable = m_TableVec[Index];
	if (!pTable)
	{
		ASSERT(false);
	}
	pTable->SendTableRoleInfoToClient(dwUserID);
}
void TableManager::OnPlayerLeaveTable(DWORD dwUserID)
{
	//��������ҵ���ǰ������ 
	//���������ҵ���� Ȼ��������뿪����
	//CWHDataLocker lock(m_CriticalSection);
	HashMap<DWORD, WORD>::iterator Iter = m_RoleTableMap.find(dwUserID);
	if (Iter == m_RoleTableMap.end())
	{
		//ASSERT(false);
		return;
	}
	WORD Index = Iter->second;
	if (m_TableVec.size() <= Index)
	{
		ASSERT(false);
	}
	GameTable* pTable = m_TableVec[Index];
	if (!pTable)
	{
		ASSERT(false);
	}

	if (pTable->GetTableMonthID() == 0)
		OnChangeTableTypePlayerSum(pTable->GetTableTypeID(), false);

	//������뿪���ӵ�ʱ�� �����ˢ���µ��ͻ���ȥ
	CRoleEx* pRole = g_FishServer.GetRoleManager()->QueryUser(dwUserID);
	if (pRole)
	{
		DBR_Cmd_TableChange msgDB;//��¼��ҽ���
		SetMsgInfo(msgDB, DBR_TableChange, sizeof(DBR_Cmd_TableChange));
		msgDB.dwUserID = pRole->GetUserID();
		msgDB.CurrceySum = pRole->GetRoleInfo().dwCurrencyNum;
		msgDB.GlobelSum = pRole->GetRoleInfo().dwGlobeNum;
		msgDB.MedalSum = pRole->GetRoleInfo().dwMedalNum;
		msgDB.JoinOrLeave = false;
		msgDB.LogTime = time(null);
		msgDB.TableTypeID = pTable->GetTableTypeID();
		msgDB.TableMonthID = pTable->GetTableMonthID();
		g_FishServer.SendNetCmdToSaveDB(&msgDB);
		g_DBLogManager.LogRoleJoinOrLeaveTableInfo(msgDB.dwUserID, msgDB.GlobelSum, msgDB.CurrceySum, msgDB.MedalSum, msgDB.TableTypeID, msgDB.TableMonthID, false, SendLogDB);

		if (pTable->GetTableMonthID() != 0)
		{
			pRole->GetRoleMonth().OnPlayerLeaveTable();//����뿪����
		}

		ServerClientData* pData = g_FishServer.GetUserClientDataByIndex(pRole->GetGameSocketID());
		if (pData)
		{
			pData->IsInScene = false;
		}
		LogInfoToFile("CollideBuYu.txt", "OnPlayerLeaveTable�����ˢ���µ��ͻ���ȥ 33322******* %d******* %d-----", pRole->GetRoleInfo().dwGlobeNum, pRole->GetUserID());


		LC_Cmd_ResetRoleGlobel msg;
		SetMsgInfo(msg, GetMsgType(Main_Role, LC_ResetRoleGlobel), sizeof(LC_Cmd_ResetRoleGlobel));
		msg.dwTotalGlobel = pRole->GetRoleInfo().dwGlobeNum;
		msg.dwLotteryScore = pRole->GetRoleInfo().LotteryScore;
		pRole->SendDataToClient(&msg);//
	}

	pTable->OnRoleLeaveTable(dwUserID);
	m_RoleTableMap.erase(Iter);//����ҵ�����ɾ����
	if (pTable->GetTableMonthID() == 0 && !pRole->IsRobot())
	{
		g_FishServer.GetRobotManager().OnRoleLeaveNormalRoom(pTable);
	}
		
	//////////////////////////////////////////////////////////////////////////��ӷ���ģʽ��ɾ������  Index
	WORD	wTableID = pTable->GetTableID();

	if (pTable->GetTableTypeID() == 0)
	{
		//��ȡ����������
		if (pTable->m_RoleManager.GetRoleSum() != 0)
		{
			return;
		}
		if (!m_TableVec.empty())
		{
			std::vector<GameTable*>::iterator Iter = m_TableVec.begin();

			for (; Iter != m_TableVec.end(); ++Iter)
			{
				if (!(*Iter))
					continue;

				if (m_TableVec[Index] != (*Iter))
				{
					continue;
				}
				//CTraceService::TraceString(TEXT("һ�����Ӷ��� �Ѿ�������"), TraceLevel_Normal);
				LogInfoToFile("CollideBuYu.txt", "һ�����Ӷ��� �Ѿ�������33322******* %d******* %d----------%d----------%d----", pRole->GetRoleInfo().dwGlobeNum, pRole->GetUserID(), Iter, Index);

				delete *Iter;

				m_TableVec.erase(Iter);
				m_privateTableIDPool.push_back(wTableID);
				//ɾ������  zheshi  ���ǲ���  
				DeletePrivateTable(m_TableVec[Index], pRole);
			}			
			//m_TableVec.clear();
			
		}

	}
	//////////////////////////////////////////////////////////////////////////

}
bool TableManager::OnHandleTableMsg(DWORD dwUserID,NetCmd* pData)
{
	//��Game��Ϣ���ݵ��ڲ�ȥ
	//CWHDataLocker lock(m_CriticalSection);
	HashMap<DWORD, WORD>::iterator Iter = m_RoleTableMap.find(dwUserID);
	if (Iter == m_RoleTableMap.end())
	{
		//ASSERT(false);
		return false;
	}
	WORD Index = Iter->second;
	if (m_TableVec.size() <= Index)
	{
		ASSERT(false);
		return false;
	}
	GameTable* pTable = m_TableVec[Index];
	if (!pTable)
	{
		ASSERT(false); 
	}
	pTable->OnHandleTableMsg(dwUserID,pData);
	return true;
}
void TableManager::SendDataToTable(DWORD dwUserID, NetCmd* pData)
{
	//������͵�ָ����ҵ���������ȥ
	HashMap<DWORD, WORD>::iterator Iter = m_RoleTableMap.find(dwUserID);
	if (Iter == m_RoleTableMap.end())
	{
		return;
	}
	WORD Index = Iter->second;
	if (m_TableVec.size() <= Index)
	{
		ASSERT(false);
		return;
	}
	GameTable* pTable = m_TableVec[Index];
	if (pTable)
	{
		pTable->SendDataToTable(dwUserID, pData);
	}
	else
	{
		ASSERT(false);
	}
}
CRole* TableManager::SearchUser(DWORD dwUserid)
{
	HashMap<DWORD, WORD>::iterator Iter = m_RoleTableMap.find(dwUserid);
	if (Iter == m_RoleTableMap.end())
	{
		//ASSERT(false);
		return NULL;
	}
	WORD Index = Iter->second;
	if (m_TableVec.size() <= Index)
	{
		ASSERT(false);
		return NULL; 
	}
	GameTable* pTable = m_TableVec[Index];
	if (!pTable)
	{
		ASSERT(false);
		return NULL;
	}
	return pTable->SearchUser(dwUserid);
}
CConfig* TableManager::GetGameConfig()
{
	return m_pGameConfig;
}
void TableManager::OnChangeTableGlobel(WORD TableID, int AddGlobel, USHORT uTableRate)
{
	
	GameTable* pTable = GetTable(TableID);
	if (!pTable)
	{
		ASSERT(false);
		return;
	}
	int64 AllGlobelSum = 0;
	//////////////////////////////////////////////////////////////////////////�޸�
	if (AddGlobel>0)
	{
		//LogInfoToFile("Control.txt", "û����:::::::::::::::%d:::::%d::::", AddGlobel, KucunNum[pTable->GetTableTypeID()]);
		KucunNum[pTable->GetTableTypeID()] += AddGlobel*0.995;
	}
	else
	{
		//LogInfoToFile("Control.txt", "����:::::::::::::::%d::::%d:::::", AddGlobel, KucunNum[pTable->GetTableTypeID()]);
		KucunNum[pTable->GetTableTypeID()] += AddGlobel;
	}

	//////////////////////////////////////////////////////////////////////////
	HashMap<BYTE, GoldPool>::iterator Iter = m_TableGlobeMap.find(pTable->GetTableTypeID());

	if (Iter == m_TableGlobeMap.end())
	{
		AllGlobelSum = AddGlobel;
		m_TableGlobeMap.insert(HashMap<BYTE, GoldPool>::value_type(pTable->GetTableTypeID(), GoldPool(AllGlobelSum)));
	}
	else
	{
		Iter->second.gold += AddGlobel;
		AllGlobelSum = Iter->second.gold;
	}	

	Iter = m_TableGlobeMap.find(pTable->GetTableTypeID());
	//{
	//	char szKey[10] = { 0 };
	//	char szValue[32] = { 0 };
	//	char szFile[256] = { 0 };
	//	sprintf_s(szKey, sizeof(szKey), "%d", pTable->GetTableTypeID());
	//	sprintf_s(szValue, sizeof(szValue), "%lld", AllGlobelSum);
	//	sprintf_s(szFile, sizeof(szFile), "%s\\%s", g_FishServerConfig.GetFTPServerConfig()->FTPFilePath, "fish.txt");
	//	WritePrivateProfileStringA("fish", szKey, szValue, szFile);
	//}	
	if (Iter->second.open)
	{
		if (AllGlobelSum <=0)
		{
			Iter->second.open = false;
			//Iter->second.gold = 0;			
		}
	}

	if(AllGlobelSum >= Con_LuckItem::s_threshold*uTableRate)//�������ص�����  1000*10
	{
		Iter->second.open = true;
		Iter->second.gold -= (int64)(Con_LuckItem::s_base*AllGlobelSum);//�ȿ�  ˥��
		byte nGiveIndex=Iter->second.byGive++;			

		DWORD UseGlobelSum = (DWORD)AllGlobelSum;// 		
		DWORD PlayerSum = 0;
		HashMap<BYTE, DWORD>::iterator Iter = m_TableTypeSum.find(pTable->GetTableTypeID());
		if (Iter == m_TableTypeSum.end())
		{
			PlayerSum = 0;
		}
		else
		{
			PlayerSum = Iter->second;
		}
		if (PlayerSum != 0 )
		{					
			DWORD RoleAddGlobel = (DWORD)min(UseGlobelSum / PlayerSum, UseGlobelSum *Con_LuckItem::s_part);
			if (nGiveIndex%2 == 0)//�ȵ��ȵ�
			{
				RoleAddGlobel = (int64)(UseGlobelSum *Con_LuckItem::s_part);
			}
			DWORD AddLuckyValue = RoleAddGlobel ;			
			//��ȫ��������ֵ������ҵ�����
			std::vector<GameTable*>::iterator IterVec = m_TableVec.begin();
			for (; IterVec != m_TableVec.end(); ++IterVec)
			{
				if (!(*IterVec))
					continue;
				if ((*IterVec)->GetTableTypeID() == pTable->GetTableTypeID() && (*IterVec)->GetTableMonthID() == 0)
				{
					BYTE MaxSum = (*IterVec)->GetRoleManager().GetMaxPlayerSum();
					for (BYTE i = 0; i < MaxSum; ++i)
					{
						CRole* pRole = (*IterVec)->GetRoleManager().GetRoleBySeatID(i);
						if (pRole && pRole->IsActionUser())
						{
							pRole->SetRoleLuckyValue(AddLuckyValue);
						}
					}
				}
			}
		}
	}
}
//�޸����ӵĿ��ֵ
void TableManager::OnResetTableGlobel(WORD TableID, int64 nValue)
{
	GameTable* pTable = GetTable(TableID);
	if (!pTable)
	{
		ASSERT(false);
		return;
	}
	HashMap<BYTE, GoldPool>::iterator Iter = m_TableGlobeMap.find(pTable->GetTableTypeID());
	if (Iter != m_TableGlobeMap.end())
	{
		Iter->second = nValue;
		LogInfoToFile("Control.txt", "OnResetTableGlobel�޸����ӵĿ��ֵ:::::::%d:::::::::::%d::::::::::::::::::::", nValue, TableID);
	}
}
int64 TableManager::GetTableGlobel(WORD TableID)
{
	GameTable* pTable = GetTable(TableID);
	if (!pTable)
	{
		ASSERT(false);
		return 0;
	}
	HashMap<BYTE, GoldPool>::iterator Iter = m_TableGlobeMap.find(pTable->GetTableTypeID());
	if (Iter != m_TableGlobeMap.end())
		return Iter->second.gold;
	else
		return 0;
}
void TableManager::OnChangeTableTypePlayerSum(BYTE TableTypeID, bool IsAddOrDel)
{
	HashMap<BYTE, DWORD>::iterator Iter = m_TableTypeSum.find(TableTypeID);
	if (Iter == m_TableTypeSum.end())
	{
		if (IsAddOrDel)
		{
			m_TableTypeSum.insert(HashMap<BYTE, DWORD>::value_type(TableTypeID, 1));
			return;
		}
		else
		{
			ASSERT(false);
			return;
		}
	}
	else
	{
		if (IsAddOrDel)
		{
			Iter->second += 1;
			return;
		}
		else
		{
			if (Iter->second == 0)
			{
				ASSERT(false);
				return;
			}
			Iter->second -= 1;
			return;
		}
	}
}

bool TableManager::QueryPool(WORD TableID,int64 & nPoolGold)
{
	GameTable* pTable = GetTable(TableID);
	if (!pTable)
	{
		ASSERT(false);
		return false;
	}
	HashMap<BYTE, GoldPool>::iterator Iter = m_TableGlobeMap.find(pTable->GetTableTypeID());
	if (Iter == m_TableGlobeMap.end())
	{
		return false;
	}
	nPoolGold = Iter->second.gold;
	return 	Iter->second.open;
}

void TableManager::QueryPool(BYTE TableTypeID, bool & bopen, int64 & nPoolGold)
{
	bopen = false;
	nPoolGold = 0;

	HashMap<BYTE, GoldPool>::iterator Iter = m_TableGlobeMap.find(TableTypeID);
	if (Iter == m_TableGlobeMap.end())
	{
		return;
	}
	bopen = Iter->second.open;
	nPoolGold = Iter->second.gold;
}

std::list<DWORD> TableManager::GetBlackList()
{
	return m_blacklist;
}

bool TableManager::SetBlackList(DWORD *pid, BYTE byCount)
{
	if (pid == NULL || byCount == 0)
	{
		return false;
	}
	m_blacklist.clear();
	for (int i = 0; i < byCount; i++)
	{
		m_blacklist.push_back(pid[i]);
	}	
	return true;
}

bool TableManager::Isabhor(DWORD dwUserid)
{
	return std::find(m_blacklist.begin(), m_blacklist.end(), dwUserid) != m_blacklist.end();
}

//�޸ĸ���
float TableManager::OnChangeGailv(float NumrateF, float NumrateS, float NumrateT, float NumrateR,bool IsContrl)
{

	if (!IsContrl)
	{
		RNumrateF = 0;
		RNumrateS = 0;
		RNumrateT = 0;
		RNumrateR = 0;
		return 0;
	}

	RNumrateF = NumrateF;
	RNumrateS = NumrateS;
	RNumrateT = NumrateT;
	RNumrateR = NumrateR;

	LogInfoToFile("Control.txt", "OnChangeGailv�޸Ĳ�����ʿ������:::::::%f:::::::::::%f::::::::::::::::::::", RNumrateF, RNumrateS);

	return NumrateF;
}

