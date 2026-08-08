#include "tetrisu.h"

#if TETRISU_ENABLE_AUDIO

// Static Functions
static void	log_mix_error(const char *context);
static void	free_music(t_audio_ctx *audio);
static bool	start_looping_music(t_audio_ctx *audio, const char *path,
				int fade_ms);
static const char	*game_sfx_path(t_audio_sfx sfx);
static int	game_sfx_volume(t_audio_sfx sfx);
static int	scaled_volume(const t_audio_ctx *audio, int base);
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
int	audio_init(t_audio_ctx *audio)
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
	Mix_AllocateChannels(16);
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
void	audio_play_music(t_audio_ctx *audio, const char *path)
{
	if (audio == NULL || !audio->enabled || path == NULL)
		return ;
#if TETRISU_ENABLE_AUDIO
	free_music(audio);
	audio->music_transition_phase = 0;
	audio->music_transition_elapsed_ms = 0;
	audio->music_transition_duration_ms = 0;
	audio->pending_music_path[0] = '\0';
	if (!start_looping_music(audio, path, 0))
		audio->music_path[0] = '\0';
#else
	(void)path;
#endif
}

/**
 * @brief Fades from the current loop into another without blocking gameplay.
 *
 * SDL_mixer exposes one streamed music channel, so the transition uses equal
 * fade-out and fade-in halves. Repeated requests for the active or pending
 * track are ignored.
 *
 * @param audio Audio context returned by audio_init().
 * @param path Music asset that should loop after the transition.
 * @param duration_ms Total transition duration in milliseconds.
 */
void	audio_transition_music(t_audio_ctx *audio, const char *path,
	int duration_ms)
{
	if (audio == NULL || !audio->enabled || path == NULL)
		return ;
#if TETRISU_ENABLE_AUDIO
	if ((audio->music_transition_phase == 0
			&& audio->music != NULL
			&& strcmp(audio->music_path, path) == 0)
		|| (duration_ms > 0 && audio->music_transition_phase != 0
			&& strcmp(audio->pending_music_path, path) == 0))
		return ;
	if (duration_ms <= 0 || audio->music == NULL)
	{
		audio_play_music(audio, path);
		return ;
	}
	snprintf(audio->pending_music_path,
		sizeof(audio->pending_music_path), "%s", path);
	audio->music_transition_elapsed_ms = 0;
	audio->music_transition_duration_ms = duration_ms;
	audio->music_transition_phase = 1;
	if (Mix_FadeOutMusic((duration_ms + 1) / 2) == 0)
		audio_play_music(audio, path);
#else
	(void)duration_ms;
#endif
}

/**
 * @brief Advances a scheduled music transition.
 *
 * @param audio Audio context returned by audio_init().
 * @param elapsed_ms Elapsed monotonic time in milliseconds.
 * @return true when a transition boundary was processed.
 */
bool	audio_update(t_audio_ctx *audio, int elapsed_ms)
{
#if TETRISU_ENABLE_AUDIO
	int	midpoint_ms;
	int	fade_in_ms;

	if (audio == NULL || !audio->enabled || elapsed_ms < 0
		|| audio->music_transition_phase == 0)
		return (false);
	if (elapsed_ms > audio->music_transition_duration_ms
		- audio->music_transition_elapsed_ms)
		audio->music_transition_elapsed_ms
			= audio->music_transition_duration_ms;
	else
		audio->music_transition_elapsed_ms += elapsed_ms;
	midpoint_ms = (audio->music_transition_duration_ms + 1) / 2;
	if (audio->music_transition_phase == 1
		&& audio->music_transition_elapsed_ms >= midpoint_ms)
	{
		free_music(audio);
		fade_in_ms = audio->music_transition_duration_ms - midpoint_ms;
		if (!start_looping_music(audio,
				audio->pending_music_path, fade_in_ms))
		{
			audio->music_transition_phase = 0;
			audio->pending_music_path[0] = '\0';
			return (true);
		}
		audio->pending_music_path[0] = '\0';
		audio->music_transition_phase = 2;
		if (audio->music_transition_elapsed_ms
			< audio->music_transition_duration_ms)
			return (true);
	}
	if (audio->music_transition_phase == 2
		&& audio->music_transition_elapsed_ms
			>= audio->music_transition_duration_ms)
	{
		audio->music_transition_phase = 0;
		audio->music_transition_elapsed_ms = 0;
		audio->music_transition_duration_ms = 0;
		return (true);
	}
	return (false);
#else
	(void)audio;
	(void)elapsed_ms;
	return (false);
#endif
}

/**
 * @brief Returns the next non-blocking music transition boundary.
 *
 * @param audio Audio context returned by audio_init().
 * @return Milliseconds until work is due, or -1 when music is stable.
 */
int	audio_next_wake_ms(const t_audio_ctx *audio)
{
	int	wake_ms;

	if (audio == NULL || !audio->enabled
		|| audio->music_transition_phase == 0)
		return (-1);
	if (audio->music_transition_phase == 1)
		wake_ms = (audio->music_transition_duration_ms + 1) / 2
			- audio->music_transition_elapsed_ms;
	else
		wake_ms = audio->music_transition_duration_ms
			- audio->music_transition_elapsed_ms;
	if (wake_ms < 0)
		return (0);
	return (wake_ms);
}

/**
 * @brief Loads and plays one music asset once, replacing current music.
 *
 * @param audio Audio context returned by audio_init().
 * @param path Music asset path to play once.
 */
void	audio_play_once(t_audio_ctx *audio, const char *path)
{
	if (audio == NULL || !audio->enabled || path == NULL)
		return ;
#if TETRISU_ENABLE_AUDIO
	free_music(audio);
	audio->music_transition_phase = 0;
	audio->music_transition_elapsed_ms = 0;
	audio->music_transition_duration_ms = 0;
	audio->music_path[0] = '\0';
	audio->pending_music_path[0] = '\0';
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
	else
		snprintf(audio->music_path, sizeof(audio->music_path), "%s", path);
#else
	(void)path;
#endif
}

/**
 * @brief Stops and unloads the currently playing music asset.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_stop_music(t_audio_ctx *audio)
{
	if (audio == NULL || !audio->enabled)
		return ;
#if TETRISU_ENABLE_AUDIO
	free_music(audio);
	audio->music_path[0] = '\0';
	audio->pending_music_path[0] = '\0';
	audio->music_transition_phase = 0;
	audio->music_transition_elapsed_ms = 0;
	audio->music_transition_duration_ms = 0;
#endif
}

/**
 * @brief Loads short menu sound effects; missing files are ignored.
 *
 * @param audio Audio context returned by audio_init().
 * @param move_path Sound played on up/down selection movement.
 * @param select_path Sound played on enter/selection confirmation.
 */
void	audio_load_menu_sfx(t_audio_ctx *audio, const char *move_path,
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
	audio_apply_effect_volume(audio);
#else
	(void)move_path;
	(void)select_path;
#endif
}

/**
 * @brief Loads the neutral gameplay clips supplied in General Sounds.
 *
 * Localized voice variants and long menu jingles remain available as assets
 * but are not loaded into the latency-sensitive gameplay bank.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_load_game_sfx(t_audio_ctx *audio)
{
#if TETRISU_ENABLE_AUDIO
	const char	*path;
	int			index;

	if (audio == NULL || !audio->enabled)
		return ;
	index = 0;
	while (index < AUDIO_SFX_COUNT)
	{
		free_chunk(&audio->game_sfx[index]);
		path = game_sfx_path((t_audio_sfx)index);
		if (path != NULL)
			audio->game_sfx[index] = Mix_LoadWAV(path);
		index++;
	}
	audio_apply_effect_volume(audio);
#else
	(void)audio;
#endif
}

/**
 * @brief Plays one preloaded gameplay effect on a free mixer channel.
 *
 * @param audio Audio context returned by audio_init().
 * @param sfx Effect identifier to play.
 */
void	audio_play_sfx(t_audio_ctx *audio, t_audio_sfx sfx)
{
	if (audio == NULL || !audio->enabled
		|| sfx < 0 || sfx >= AUDIO_SFX_COUNT)
		return ;
#if TETRISU_ENABLE_AUDIO
	play_chunk(audio->game_sfx[sfx]);
#endif
}

/**
 * @brief Plays the menu movement sound effect if loaded.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_play_menu_move(t_audio_ctx *audio)
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
void	audio_play_menu_select(t_audio_ctx *audio)
{
	if (audio == NULL || !audio->enabled)
		return ;
#if TETRISU_ENABLE_AUDIO
	play_chunk(audio->menu_select_sfx);
#endif
}

/**
 * @brief Plays the short, restrained cue used when entering a room.
 *
 * The existing 300 ms dialog clip is intentionally reused: its mix level is
 * 48 rather than the normal gameplay level of 72, so the acknowledgement is
 * audible without competing with room chat or music.
 */
void	audio_play_room_entry(t_audio_ctx *audio)
{
	audio_play_menu_select(audio);
}

/**
 * @brief Sets music volume to an absolute level, clamped to the mixer range.
 *
 * @param audio Audio context returned by audio_init().
 * @param volume Target volume; later +/- steps adjust from this level.
 */
void	audio_set_music_volume(t_audio_ctx *audio, int volume)
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
	audio_apply_effect_volume(audio);
}


/**
 * @brief Rescales every loaded effect to follow the single volume control.
 *
 * There is one volume in the UI, so effects track it rather than sitting at
 * fixed levels: turning the music down and still being shouted at by the menu
 * is worse than having no control at all. The per-effect constants stay as the
 * mix balance and are scaled together, so at full volume nothing changes.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_apply_effect_volume(t_audio_ctx *audio)
{
#if TETRISU_ENABLE_AUDIO
	int	index;

	if (audio == NULL || !audio->enabled)
		return ;
	if (audio->menu_move_sfx != NULL)
		Mix_VolumeChunk((Mix_Chunk *)audio->menu_move_sfx,
			scaled_volume(audio, AUDIO_MENU_MOVE_VOLUME));
	if (audio->menu_select_sfx != NULL)
		Mix_VolumeChunk((Mix_Chunk *)audio->menu_select_sfx,
			scaled_volume(audio, AUDIO_MENU_SELECT_VOLUME));
	index = 0;
	while (index < AUDIO_SFX_COUNT)
	{
		if (audio->game_sfx[index] != NULL)
			Mix_VolumeChunk((Mix_Chunk *)audio->game_sfx[index],
				scaled_volume(audio, game_sfx_volume((t_audio_sfx)index)));
		index++;
	}
#else
	(void)audio;
#endif
}

/**
 * @brief Raises music volume by one fixed step.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_volume_up(t_audio_ctx *audio)
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
	audio_apply_effect_volume(audio);
}

/**
 * @brief Lowers music volume by one fixed step.
 *
 * @param audio Audio context returned by audio_init().
 */
void	audio_volume_down(t_audio_ctx *audio)
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
	audio_apply_effect_volume(audio);
}

/**
 * @brief Releases audio assets and closes the audio device.
 *
 * @param audio Audio context to tear down.
 */
void	audio_teardown(t_audio_ctx *audio)
{
	if (audio == NULL)
		return ;
#if TETRISU_ENABLE_AUDIO
	if (audio->enabled)
	{
		int	index;

		free_music(audio);
		free_chunk(&audio->menu_move_sfx);
		free_chunk(&audio->menu_select_sfx);
		index = 0;
		while (index < AUDIO_SFX_COUNT)
		{
			free_chunk(&audio->game_sfx[index]);
			index++;
		}
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
static void	free_music(t_audio_ctx *audio)
{
	if (audio->music != NULL)
	{
		Mix_HaltMusic();
		Mix_FreeMusic((Mix_Music *)audio->music);
		audio->music = NULL;
	}
}

/**
 * @brief Loads one looping music stream, optionally fading it in.
 *
 * @param audio Audio context that owns the stream.
 * @param path Music asset path.
 * @param fade_ms Fade-in duration; zero starts immediately.
 * @return true when the stream loaded and started.
 */
static bool	start_looping_music(t_audio_ctx *audio, const char *path,
	int fade_ms)
{
	int	result;

	audio->music = Mix_LoadMUS(path);
	if (audio->music == NULL)
	{
		log_mix_error("Mix_LoadMUS failed");
		return (false);
	}
	Mix_VolumeMusic(audio->music_volume);
	if (fade_ms > 0)
		result = Mix_FadeInMusic((Mix_Music *)audio->music, -1, fade_ms);
	else
		result = Mix_PlayMusic((Mix_Music *)audio->music, -1);
	if (result != 0)
	{
		log_mix_error("Mix_PlayMusic failed");
		free_music(audio);
		return (false);
	}
	snprintf(audio->music_path, sizeof(audio->music_path), "%s", path);
	return (true);
}

/**
 * @brief Maps gameplay effect identifiers to the supplied WAV files.
 *
 * @param sfx Effect identifier.
 * @return Asset path, or NULL for an invalid identifier.
 */
static const char	*game_sfx_path(t_audio_sfx sfx)
{
	static const char	*paths[AUDIO_SFX_COUNT] = {
		GENERAL_SFX_DIR "/se_game_move.wav",
		GENERAL_SFX_DIR "/se_game_rotate.wav",
		GENERAL_SFX_DIR "/se_game_softdrop.wav",
		GENERAL_SFX_DIR "/se_game_harddrop.wav",
		GENERAL_SFX_DIR "/se_game_landing.wav",
		GENERAL_SFX_DIR "/se_game_hold.wav",
		GENERAL_SFX_DIR "/se_game_single.wav",
		GENERAL_SFX_DIR "/se_game_double.wav",
		GENERAL_SFX_DIR "/se_game_triple.wav",
		GENERAL_SFX_DIR "/se_game_tetris.wav",
		GENERAL_SFX_DIR "/se_game_perfect.wav",
		GENERAL_SFX_DIR "/se_game_special.wav",
		GENERAL_SFX_DIR "/se_game_attack1.wav",
		GENERAL_SFX_DIR "/se_sys_alert.wav",
		GENERAL_SFX_DIR "/se_game_count.wav",
		GENERAL_SFX_DIR "/me_game_start2.wav",
		GENERAL_SFX_DIR "/se_game_pause.wav",
		GENERAL_SFX_DIR "/me_game_plvup.wav",
		GENERAL_SFX_DIR "/me_game_iget.wav"
	};

	if (sfx < 0 || sfx >= AUDIO_SFX_COUNT)
		return (NULL);
	return (paths[sfx]);
}

/**
 * @brief Returns a restrained per-category effect volume.
 *
 * High-frequency movement sounds stay quiet beneath the music; event and
 * achievement cues use the normal effect level.
 *
 * @param sfx Effect identifier.
 * @return SDL_mixer chunk volume.
 */
/**
 * @brief Scales one mix-balance constant by the current volume setting.
 */
static int	scaled_volume(const t_audio_ctx *audio, int base)
{
	if (audio == NULL || AUDIO_MAX_VOLUME <= 0)
		return (0);
	return (base * audio->music_volume / AUDIO_MAX_VOLUME);
}

static int	game_sfx_volume(t_audio_sfx sfx)
{
	if (sfx == AUDIO_SFX_MOVE || sfx == AUDIO_SFX_ROTATE
		|| sfx == AUDIO_SFX_SOFT_DROP)
		return (AUDIO_SFX_QUIET_VOLUME);
	return (AUDIO_SFX_NORMAL_VOLUME);
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
