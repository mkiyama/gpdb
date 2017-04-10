#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../syslogger.c"

time_t
time(time_t *unused)
{
    return (time_t)mock();
}


void
test__open_alert_log_file__NonGucOpen(void **state)
{
    gpperfmon_log_alert_level = GPPERFMON_LOG_ALERT_LEVEL_NONE;
    open_alert_log_file();
    assert_false(alert_log_level_opened);
}

void 
test__open_alert_log_file__NonMaster(void **state)
{
    Gp_entry_postmaster = false;
    gpperfmon_log_alert_level = GPPERFMON_LOG_ALERT_LEVEL_WARNING;
    open_alert_log_file();
    assert_false(alert_log_level_opened);
}

void 
test__logfile_getname(void **state)
{
    char *alert_file_name;

    alert_file_pattern = "alert_log";
    will_return(time, 12345);

    alert_file_name = logfile_getname(time(NULL), NULL, "gpperfmon/logs", "alert_log");
    assert_true(strcmp(alert_file_name, "gpperfmon/logs/alert_log.12345") == 0);
}

int
main(int argc, char* argv[]) {
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
    		unit_test(test__open_alert_log_file__NonGucOpen),
    		unit_test(test__open_alert_log_file__NonMaster),
    		unit_test(test__logfile_getname)
    };

	MemoryContextInit();

    return run_tests(tests);
}
