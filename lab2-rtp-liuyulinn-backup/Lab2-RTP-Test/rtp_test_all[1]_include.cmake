if(EXISTS "/home/liuyulin/Documents/CourseLab/web/lab2-rtp-liuyulinn/Lab2-RTP-Test/rtp_test_all[1]_tests.cmake")
  include("/home/liuyulin/Documents/CourseLab/web/lab2-rtp-liuyulinn/Lab2-RTP-Test/rtp_test_all[1]_tests.cmake")
else()
  add_test(rtp_test_all_NOT_BUILT rtp_test_all_NOT_BUILT)
endif()
