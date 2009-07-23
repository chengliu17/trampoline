/*Instance of task t1*/

#include "embUnit.h"
#include "Os.h"

DeclareScheduleTable(sched_explicit);
DeclareCounter(Software_Counter1);

/*test case:test the reaction of the system called with 
 an activation of a task*/
static void test_t1_instance(void)
{
	StatusType result_inst_1, result_inst_2, result_inst_3, result_inst_4, result_inst_5, result_inst_6, result_inst_7, result_inst_8, result_inst_9, result_inst_10, result_inst_11, result_inst_12, result_inst_13, result_inst_14, result_inst_15, result_inst_16;
	ScheduleTableStatusType ScheduleTableStatusType_inst_1, ScheduleTableStatusType_inst_2, ScheduleTableStatusType_inst_3;
	
	SCHEDULING_CHECK_INIT(1);
	result_inst_1 = StartScheduleTableSynchron(sched_explicit);
	SCHEDULING_CHECK_AND_EQUAL_INT(1,E_OK, result_inst_1);
		
	SCHEDULING_CHECK_INIT(2);
	result_inst_2 = GetScheduleTableStatus(sched_explicit, &ScheduleTableStatusType_inst_1);
	SCHEDULING_CHECK_AND_EQUAL_INT_FIRST(2, SCHEDULETABLE_WAITING , ScheduleTableStatusType_inst_1);
	SCHEDULING_CHECK_AND_EQUAL_INT(2,E_OK, result_inst_2);
	
	SCHEDULING_CHECK_INIT(3);
	result_inst_3 = StartScheduleTableSynchron(sched_explicit);
	SCHEDULING_CHECK_AND_EQUAL_INT(3,E_OS_STATE, result_inst_3);
	
	SCHEDULING_CHECK_INIT(4);
	result_inst_4 = IncrementCounter(Software_Counter1);
	/*offset = 1 */
	SCHEDULING_CHECK_AND_EQUAL_INT(4,E_OK, result_inst_4);
	
	SCHEDULING_CHECK_INIT(5);
	result_inst_5 = IncrementCounter(Software_Counter1);
	/*offset = 2 */
	SCHEDULING_CHECK_AND_EQUAL_INT(5,E_OK, result_inst_5);
	
	SCHEDULING_CHECK_INIT(6);
	result_inst_6 = GetScheduleTableStatus(sched_explicit, &ScheduleTableStatusType_inst_2);
	SCHEDULING_CHECK_AND_EQUAL_INT_FIRST(6, SCHEDULETABLE_WAITING , ScheduleTableStatusType_inst_2);
	SCHEDULING_CHECK_AND_EQUAL_INT(6,E_OK, result_inst_6);
	
	SCHEDULING_CHECK_INIT(7);
	result_inst_7 = SyncScheduleTable(sched_explicit, 3);
	SCHEDULING_CHECK_AND_EQUAL_INT(7,E_OK, result_inst_7);
	
	SCHEDULING_CHECK_INIT(8);
	result_inst_8 = GetScheduleTableStatus(sched_explicit, &ScheduleTableStatusType_inst_3);
	SCHEDULING_CHECK_AND_EQUAL_INT_FIRST(8, SCHEDULETABLE_RUNNING_AND_SYNCHRONOUS , ScheduleTableStatusType_inst_3);
	SCHEDULING_CHECK_AND_EQUAL_INT(8,E_OK, result_inst_8);
	
	SCHEDULING_CHECK_INIT(9);
	result_inst_9 = StartScheduleTableSynchron(sched_explicit);
	SCHEDULING_CHECK_AND_EQUAL_INT(9,E_OS_STATE, result_inst_9);
	
	SCHEDULING_CHECK_INIT(10);
	result_inst_10 = IncrementCounter(Software_Counter1);
	/*offset = 3 (sc) - 4 (st) */
	SCHEDULING_CHECK_AND_EQUAL_INT(10,E_OK, result_inst_10);
	
	SCHEDULING_CHECK_INIT(11);
	result_inst_11 = IncrementCounter(Software_Counter1);
	/*offset = 4 (sc) - 5=0 (st) */
	SCHEDULING_CHECK_AND_EQUAL_INT(11,E_OK, result_inst_11);

	SCHEDULING_CHECK_INIT(12);
	result_inst_12 = IncrementCounter(Software_Counter1);
	/*offset = 5 (sc) - 1 (st) -> t2 activated */
	SCHEDULING_CHECK_AND_EQUAL_INT(13,E_OK, result_inst_12);

	SCHEDULING_CHECK_INIT(14);
	result_inst_13 = IncrementCounter(Software_Counter1);
	/*offset = 6 (sc) - 2 (st) */
	SCHEDULING_CHECK_AND_EQUAL_INT(14,E_OK, result_inst_13);

	SCHEDULING_CHECK_INIT(15);
	result_inst_14 = IncrementCounter(Software_Counter1);
	/*offset = 7 (sc) - 3 (st) -> t2 receive event */
	SCHEDULING_CHECK_AND_EQUAL_INT(16,E_OK, result_inst_14);

	SCHEDULING_CHECK_INIT(17);
	result_inst_15 = IncrementCounter(Software_Counter1);
	/*offset = 8 (sc) - 4 (st) ->  */
	SCHEDULING_CHECK_AND_EQUAL_INT(17,E_OK, result_inst_15);
	
	SCHEDULING_CHECK_INIT(18);
	result_inst_16 = IncrementCounter(Software_Counter1);
	/*offset = 9 (sc) - 5=0 (st) ->  */
	SCHEDULING_CHECK_AND_EQUAL_INT(18,E_OK, result_inst_16);
	
}

/*create the test suite with all the test cases*/
TestRef AutosarSTSTest_seq3_t1_instance(void)
{
	EMB_UNIT_TESTFIXTURES(fixtures) {
		new_TestFixture("test_t1_instance",test_t1_instance)
	};
	EMB_UNIT_TESTCALLER(AutosarSTSTest,"AutosarSTTest_sequence3",NULL,NULL,fixtures);
	
	return (TestRef)&AutosarSTSTest;
}
