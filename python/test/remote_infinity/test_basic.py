import pytest
from common import common_values
from src.test_basic import TestCase

class TestRemoteInfinity():
    def test_basic(self):
        test_case_obj = TestCase(common_values.TEST_REMOTE_HOST)
        test_case_obj._test_version()
        test_case_obj._test_connection()
        test_case_obj._test_create_db_with_invalid_name()
        test_case_obj._test_basic()