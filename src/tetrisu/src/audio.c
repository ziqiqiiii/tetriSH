#include "tetrisu.h"

#if TETRISU_ENABLE_AUDIO

// Static Functions
static void	log_mix_error(const char *context);
static void	free_music(audio_ctx_t *audio);
static void	free_chunk(void **chunk);
static void	play_chunk(void *chunk);

#endif

/**
 * @brief Initialises optional SDL2_mixer audio for music and menu SFX.
 *
 * AI-assisted: audio is best-effort because WSL/headless terminals often have
 * no usable audio device; failure leaves the render/input path untouched.
 *
 * @param audio Audio context populated by this function.
 * @return 1 when SDL2_mixer is available and opened, 0 for silent fallback.
 */
int	audio_init(audio_ctx_t *audio)
{
	if (audio == NULL)
		return (0);
	memset(audio, 0, sizeof(*audio));
	audio->music_volume = AUDIO_DEFAULT_VOLUME;
#if TETRISU_ENABLE_AUDIO
	if (SDL_Init(SDL_INIT_AUDIO) != 0)
	{
		fprintf(stderr, "tetrisu: audio SDL_Init failed: %s\n", SDL_GetError());
		return (0);
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0)
	{
		log_mix_error("Mix_OpenAudio failed");
		SDL_Quit();
		return (0);
	}
	if ((Mix_Init(MIX_INIT_MP3) & MIX_INIT_MP3) == 0)
		log_mix_error("MP3 decoder init failed");
	Mix_AllocateChannels(8);
	audio->enabled = 1;
	Mix_VolumeMusic(audio->music_volume);
	return (1);
#else
	return (0);
#endif
}

/**
 * @brief Loads and loops background music from path.
 *
 * @param audio Audio context returned by audio_init().
 * @param path Music asset path, preferably .ogg for portability.
 */
void	audio_play_music(audio_ctx_t *audio, const char *path)
{
	if (audio == NULL || !audio->enabled || path == NULL)
		return ;
#if TETRISU_ENABLE_AUDIO
	free_music(audio);
	audio->music = Mix_LoadMUS(path);
	if (audio->music == NULL)
	{
		log_mix_error("Mix_LoadMUS failed");
		return ;
	}
	Mix_VolumeMusic(audio->music_volume);
	if (Mix_PlayMusic((Mix_Music *)audio->music, -1) != 0)
	{
		log_mix_error("Mix_PlayMusic failed");
		free_music(audio);
	}
#else
	(void)path;
#endif
}

/**
 * @brief Loads and plays one music asset once, replacing current music.
 *
 * @param audio Audio context returned by audio_init().
 * @param path Music asset path to play once.
 */
void	audio_play_once(audio_ctx_t *audio, const char *path)
{
	if (audio == NULL || !audio->enabled || path == NULL)
		return ;
#if TETRISU_ENABLE_AUDIO
	free_music(audio);
	audio->music = Mix_LoadMUS(path);
	if (audio->music == NULL)
	{
		log_mix_error("Mix_LoadMUS failed");
		return ;
	}
	Mix_VolumeMusic(audio->music_volume);
	if (Mix_PlayMusic((Mix_Music *)audio->music, 0) != 0)
	{
		log_mix_error("Mix_PlayMusic failed");
		free_music(audio);
	}
#else
	(void)path;
#endif
}

/**
 * @brief Stops and unloads the currently playing music asset.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_stop_music(audio_ctx_t *audio)
{
	if (audio == NULL || !audio->enabled)
		return ;
#if TETRISU_ENABLE_AUDIO
	free_music(audio);
#endif
}

/**
 * @brief Loads short menu sound effects; missing files are ignored.
 *
 * @param audio Audio context returned by audio_init().
 * @param move_path Sound played on up/down selection movement.
 * @param select_path Sound played on enter/selection confirmation.
 */
void	audio_load_menu_sfx(audio_ctx_t *audio, const char *move_path,
	const char *select_path)
{
	if (audio == NULL || !audio->enabled)
		return ;
#if TETRISU_ENABLE_AUDIO
	free_chunk(&audio->menu_move_sfx);
	free_chunk(&audio->menu_select_sfx);
	if (move_path != NULL)
		audio->menu_move_sfx = Mix_LoadWAV(move_path);
	if (select_path != NULL)
		audio->menu_select_sfx = Mix_LoadWAV(select_path);
#else
	(void)move_path;
	(void)select_path;
#endif
}

/**
 * @brief Plays the menu movement sound effect if loaded.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_play_menu_move(audio_ctx_t *audio)
{
	if (audio == NULL || !audio->enabled)
		return ;
#if TETRISU_ENABLE_AUDIO
	play_chunk(audio->menu_move_sfx);
#endif
}

/**
 * @brief Plays the menu selection sound effect if loaded.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_play_menu_select(audio_ctx_t *audio)
{
	if (audio == NULL || !audio->enabled)
		return ;
#if TETRISU_ENABLE_AUDIO
	play_chunk(audio->menu_select_sfx);
#endif
}

/**
 * @brief Sets music volume to an absolute level, clamped to the mixer range.
 *
 * @param audio Audio context returned by audio_init().
 * @param volume Target volume; later +/- steps adjust from this level.
 */
void	audio_set_music_volume(audio_ctx_t *audio, int volume)
{
	if (audio == NULL)
		return ;
	if (volume < 0)
		volume = 0;
	if (volume > AUDIO_MAX_VOLUME)
		volume = AUDIO_MAX_VOLUME;
	audio->music_volume = volume;
#if TETRISU_ENABLE_AUDIO
	if (audio->enabled)
		Mix_VolumeMusic(audio->music_volume);
#endif
}

/**
 * @brief Raises music volume by one fixed step.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_volume_up(audio_ctx_t *audio)
{
	if (audio == NULL)
		return ;
	audio->music_volume += AUDIO_VOLUME_STEP;
	if (audio->music_volume > AUDIO_MAX_VOLUME)
		audio->music_volume = AUDIO_MAX_VOLUME;
#if TETRISU_ENABLE_AUDIO
	if (audio->enabled)
		Mix_VolumeMusic(audio->music_volume);
#endif
}

/**
 * @brief Lowers music volume by one fixed step.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_volume_down(audio_ctx_t *audio)
{
	if (audio == NULL)
		return ;
	audio->music_volume -= AUDIO_VOLUME_STEP;
	if (audio->music_volume < 0)
		audio->music_volume = 0;
#if TETRISU_ENABLE_AUDIO
	if (audio->enabled)
		Mix_VolumeMusic(audio->music_volume);
#endif
}

/**
 * @brief Releases audio assets and closes the audio device.
 *
 * @param audio Audio context to tear down.
 */
void	audio_teardown(audio_ctx_t *audio)
{
	if (audio == NULL)
		return ;
#if TETRISU_ENABLE_AUDIO
	if (audio->enabled)
	{
		free_music(audio);
		free_chunk(&audio->menu_move_sfx);
		free_chunk(&audio->menu_select_sfx);
		Mix_CloseAudio();
		Mix_Quit();
		SDL_Quit();
	}
#endif
	memset(audio, 0, sizeof(*audio));
}

#if TETRISU_ENABLE_AUDIO

/**
 * @brief Reports an SDL_mixer failure with operation context.
 *
 * @param context Short description of the failed audio operation.
 */
static void	log_mix_error(const char *context)
{
	fprintf(stderr, "tetrisu: audio %s: %s\n", context, Mix_GetError());
}

/**
 * @brief Stops and releases the currently owned music stream.
 *
 * @param audio Audio context whose music pointer is cleared.
 */
static void	free_music(audio_ctx_t *audio)
{
	if (audio->music != NULL)
	{
		Mix_HaltMusic();
		Mix_FreeMusic((Mix_Music *)audio->music);
		audio->music = NULL;
	}
}

/**
 * @brief Releases one optional menu sound effect.
 *
 * @param chunk Address of the owned chunk pointer to clear.
 */
static void	free_chunk(void **chunk)
{
	if (*chunk != NULL)
	{
		Mix_FreeChunk((Mix_Chunk *)*chunk);
		*chunk = NULL;
	}
}

/**
 * @brief Plays one menu sound without blocking the input loop.
 *
 * @param chunk Optional SDL_mixer chunk to play once.
 */
static void	play_chunk(void *chunk)
{
	if (chunk != NULL)
		Mix_PlayChannel(-1, (Mix_Chunk *)chunk, 0);
}

#endif
