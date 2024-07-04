#include <sdk.h>
#include <greatest_api.h>
#include "playtts.h"

void test_tts_play()
{

	uint16_t i;
	uint8_t data[5] = { TTS_WELCOME,          
						TTS_POWEROFF,             
						TTS_STANDBY,
						TTS_WAKEUP,
						TTS_BATLOW  };
	for (i = 0; i < 20; i++)
	{
		play_tts(data, i % 4 + 1, TTS_MODE_NOBLOCK, NULL);
	}
	play_tts(ONE_TTS(TTS_BT_CONNECTED2), 1, TTS_MODE_BLOCK, NULL);
	
	while(1);
}



TEST tts_play_tests(void){
    test_tts_play();
}

RUN_TEST_CASE("tts_test")
{
    RUN_TEST(tts_play_tests);
}

