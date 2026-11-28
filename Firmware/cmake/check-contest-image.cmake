# 从最终 ELF 的全局符号中核对比赛入口。链接器已移除不可达代码，缺少题目函数时停止打包。
execute_process(COMMAND "${NM}" --defined-only --extern-only --format=posix "${ELF}"
  OUTPUT_VARIABLE symbols COMMAND_ERROR_IS_FATAL ANY)
foreach(entry IN ITEMS position_begin position_begin_target position_check_level position_update
                       lap_start_distance lap_update contest_distance contest_time_limit)
  if(NOT "\n${symbols}" MATCHES "\n${entry} T ")
    message(FATAL_ERROR "Competition application is missing ${entry}: ${ELF}")
  endif()
endforeach()
message(STATUS "比赛入口已链接：第一题移球，第二题整圈，第三题 A-B 直线，第四题和发挥一载球循线")
