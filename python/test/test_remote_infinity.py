import pytest

from common import common_values
from infinity.common import ConflictType

from src.test_basic import TestCase
from src.test_condition import TestCondition
from src.test_connect import TestConnection
from src.test_connection_pool import TestConnectionPool
from src.test_convert import TestConvert
from src.test_database import TestDatabase

class TestRemoteInfinity():
    def test_basic(self):
        test_case_obj = TestCase(common_values.TEST_REMOTE_HOST)
        test_case_obj._test_version()
        test_case_obj._test_connection()
        test_case_obj._test_create_db_with_invalid_name()
        test_case_obj._test_basic()

    def test_condition(self):
        test_condition_obj = TestCondition(common_values.TEST_REMOTE_HOST)
        test_condition_obj._test_traverse_conditions()

    def test_connect(self):
        test_connection_obj = TestConnection(common_values.TEST_REMOTE_HOST)
        test_connection_obj._test_connect_and_disconnect_ok()
        test_connection_obj._test_connect_invalid_address()
        test_connection_obj._test_repeat_connect()
        test_connection_obj._test_multiple_connect()
        test_connection_obj._test_repeat_disconnect()

    def test_connection_pool(self):
        test_connection_pool_obj = TestConnectionPool(common_values.TEST_REMOTE_HOST)
        test_connection_pool_obj._test_basic()
        # test_connection_pool_obj._test_time_out()

    def test_convert(self):
        test_convert_obj = TestConvert(common_values.TEST_REMOTE_HOST)
        test_convert_obj._test_to_pl()
        test_convert_obj._test_to_pa()
        test_convert_obj._test_to_df()
        test_convert_obj._test_without_output_select_list()

    @pytest.mark.parametrize("condition_list", ["c1 > 0.1 and c2 < 3.0",
                                                "c1 > 0.1 and c2 < 1.0",
                                                "c1 < 0.1 and c2 < 1.0",
                                                "c1",
                                                "c1 = 0",
                                                "_row_id",
                                                "*"])
    def test_convert_test_with_valid_select_list_output(self, condition_list):
        test_convert_obj = TestConvert(common_values.TEST_REMOTE_HOST)
        test_convert_obj._test_with_valid_select_list_output(condition_list)

    @pytest.mark.parametrize("condition_list", [pytest.param("c1 + 0.1 and c2 - 1.0", ),
                                                pytest.param("c1 * 0.1 and c2 / 1.0", ),
                                                pytest.param("c1 > 0.1 %@#$sf c2 < 1.0", ),
                                                ])
    def test_convert_test_with_invalid_select_list_output(self, condition_list):
        test_convert_obj = TestConvert(common_values.TEST_REMOTE_HOST)
        test_convert_obj._test_with_invalid_select_list_output(condition_list)

    @pytest.mark.parametrize("filter_list", [
        "c1 > 10",
        "c2 > 1",
        "c1 > 0.1 and c2 < 3.0",
        "c1 > 0.1 and c2 < 1.0",
        "c1 < 0.1 and c2 < 1.0",
        "c1 < 0.1 and c1 > 1.0",
        "c1 = 0",
    ])
    def test_convert_test_output_with_valid_filter_function(self, filter_list):
        test_convert_obj = TestConvert(common_values.TEST_REMOTE_HOST)
        test_convert_obj._test_output_with_valid_filter_function(filter_list)

    @pytest.mark.parametrize("filter_list", [
        pytest.param("c1"),
        pytest.param("_row_id"),
        pytest.param("*"),
        pytest.param("#@$%@#f"),
        pytest.param("c1 + 0.1 and c2 - 1.0"),
        pytest.param("c1 * 0.1 and c2 / 1.0"),
        pytest.param("c1 > 0.1 %@#$sf c2 < 1.0"),
    ])
    def test_convert_test_output_with_invalid_filter_function(self, filter_list):
        test_convert_obj = TestConvert(common_values.TEST_REMOTE_HOST)
        test_convert_obj._test_output_with_invalid_filter_function(filter_list)

    def test_database(self):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_version()
        test_database_obj._test_database()
        test_database_obj._test_create_database_invalid_name()
        test_database_obj._test_create_drop_show_1K_databases()
        test_database_obj._test_repeatedly_create_drop_show_databases()
        test_database_obj._test_drop_database_with_invalid_name()
        test_database_obj._test_get_db()
        test_database_obj._test_db_ops_after_disconnection()
        test_database_obj._test_drop_non_existent_db()
        test_database_obj._test_get_drop_db_with_two_threads()
        test_database_obj._test_create_same_db_in_different_threads()
        test_database_obj._test_show_database()

    @pytest.mark.slow
    def test_database_test_create_drop_show_1M_databases(self):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_create_drop_show_1M_databases()

    @pytest.mark.parametrize("conflict_type", [ConflictType.Error,
                                               ConflictType.Ignore,
                                               ConflictType.Replace,
                                               0,
                                               1,
                                               2,
                                               ])
    def test_database_test_create_with_valid_option(self, conflict_type):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_create_with_valid_option(conflict_type)

    @pytest.mark.parametrize("conflict_type", [pytest.param(1.1),
                                               pytest.param("#@$@!%string"),
                                               pytest.param([]),
                                               pytest.param({}),
                                               pytest.param(()),
                                               ])
    def test_database_test_create_with_invalid_option(self, conflict_type):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_create_with_invalid_option(conflict_type)

    @pytest.mark.parametrize("conflict_type", [
        ConflictType.Error,
        ConflictType.Ignore,
        0,
        1,
    ])
    def test_database_test_drop_option_with_valid_option(self, conflict_type):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_drop_option_with_valid_option(conflict_type)

    @pytest.mark.parametrize("conflict_type", [
        pytest.param(ConflictType.Replace),
        pytest.param(2),
        pytest.param(1.1),
        pytest.param("#@$@!%string"),
        pytest.param([]),
        pytest.param({}),
        pytest.param(()),
    ])
    def test_database_test_drop_option(self, conflict_type):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_drop_option(conflict_type)

    # get_infinity_db how to init
    @pytest.mark.skip()
    @pytest.mark.parametrize("table_name", ["test_show_table"])
    def test_database_test_show_valid_table(self, get_infinity_db, table_name):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_show_valid_table(get_infinity_db, table_name)

    @pytest.mark.skip()
    @pytest.mark.parametrize("table_name", [pytest.param("Invalid name"),
                                            pytest.param(1),
                                            pytest.param(1.1),
                                            pytest.param(True),
                                            pytest.param([]),
                                            pytest.param(()),
                                            pytest.param({}),
                                            ])
    def test_database_test_show_invalid_table(self, get_infinity_db, table_name):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_show_invalid_table(get_infinity_db, table_name)

    @pytest.mark.skip()
    @pytest.mark.parametrize("table_name", [pytest.param("not_exist_name")])
    def test_database_test_show_not_exist_table(self, get_infinity_db, table_name):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_show_not_exist_table(get_infinity_db, table_name)

    @pytest.mark.skip()
    @pytest.mark.parametrize("column_name", ["test_show_table_columns"])
    def test_database_test_show_table_columns_with_valid_name(self, get_infinity_db, column_name):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_show_table_columns_with_valid_name(get_infinity_db, column_name)

    @pytest.mark.skip()
    @pytest.mark.parametrize("column_name", [pytest.param("Invalid name"),
                                             pytest.param("not_exist_name"),
                                             pytest.param(1),
                                             pytest.param(1.1),
                                             pytest.param(True),
                                             pytest.param([]),
                                             pytest.param(()),
                                             pytest.param({}),
                                             ])
    def test_database_test_show_table_columns_with_invalid_name(self, get_infinity_db, column_name):
        test_database_obj = TestDatabase(common_values.TEST_REMOTE_HOST)
        test_database_obj._test_show_table_columns_with_invalid_name(get_infinity_db, column_name)