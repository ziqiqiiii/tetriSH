# tetriSH — Class Diagrams

C has no classes, so each diagram models the code as UML classes using a consistent convention:

- **structs** → classes with their fields as attributes.
- **`.c` modules** with no owning struct → classes stereotyped `«module»` whose "methods" are their public functions (grouped by the source file that implements them).
- **enums** → classes stereotyped `«enum»`.
- Relationships: 
    - `*-->` composition (owns/embeds)
    - `o-->` aggregation (references, does not own)
    - `..>` dependency (uses/produces)
    - `--|>` realization (module implements header API).

Diagrams cover every library (`lib/*`) and every source tree (`src/*`).

Rendered PNGs live alongside this file in [`docs/img/`](img/).

---

## 1. `libtetrisbrain` — pure game logic


```mermaid
classDiagram
    direction LR

    class t_cell_type {
        <<enum>>
        CELL_EMPTY
        CELL_FILLED
        CELL_GARBAGE
    }
    class t_piece_type {
        <<enum>>
        PIECE_I
        PIECE_O
        PIECE_T
        PIECE_S
        PIECE_Z
        PIECE_J
        PIECE_L
    }
    class t_brain_result {
        <<enum>>
        BRAIN_OK
        BRAIN_BLOCKED
        BRAIN_LOCKED
        BRAIN_GAME_OVER
        BRAIN_CLEARED
    }

    class t_cell {
        +t_cell_type type
        +uint8_t color
    }
    class t_board {
        +t_cell cells[20][10]
    }
    class t_piece {
        +t_piece_type type
        +int col
        +int row
        +int rotation
    }

    class board {
        <<module>> board.c
        +board_init(t_board*)
        +board_get(t_board*, col, row) t_cell
        +board_set(t_board*, col, row, t_cell)
        +board_in_bounds(col, row) bool
        +board_inject_garbage(t_board*, lines, hole_col)
        +board_copy(dst, src)
    }
    class pieces {
        <<module>> pieces.c
        +piece_spawn(type) t_piece
        +piece_is_valid(t_board*, t_piece*) bool
        +piece_move(t_board*, t_piece*, dcol, drow) t_brain_result
        +piece_rotate(t_board*, t_piece*, dir) t_brain_result
        +piece_stamp(t_board*, t_piece*)
    }
    class gravity {
        <<module>> gravity.c
        +gravity_tick(t_board*, t_piece*) t_brain_result
        +piece_soft_drop(t_board*, t_piece*) t_brain_result
        +piece_hard_drop(t_board*, t_piece*)
    }
    class lineclear {
        <<module>> lineclear.c
        +board_clear_lines(t_board*) int
    }
    class scoring {
        <<module>> scoring.c
        +score_on_clear(lines, level) int
        +level_from_lines(total) int
        +gravity_interval_ms(level) int
    }
    class abilities {
        <<module>> abilities.c
        +board_cut_top(t_board*, n)
        +board_cut_bottom(t_board*, n)
        +board_apply_gravity(t_board*)
        +board_invert(t_board*)
        +board_fill_rows(t_board*, n, hole_col)
        +board_clear_cells(t_board*, cols[], rows[], count)
        +board_delete_columns(t_board*, start, end)
    }

    t_board *--> "200" t_cell : embeds
    t_cell o--> t_cell_type
    t_piece o--> t_piece_type

    board ..> t_board
    board ..> t_cell
    pieces ..> t_board
    pieces ..> t_piece
    pieces ..> t_brain_result
    gravity ..> t_board
    gravity ..> t_piece
    gravity ..> t_brain_result
    lineclear ..> t_board
    abilities ..> t_board
```

---

## 2. `libmacminidb` — in-memory player DB (WAL + hashmap + skiplist)


```mermaid
classDiagram
    direction LR

    class t_db_result {
        <<enum>>
        DB_OK
        DB_NOT_FOUND
        DB_EXISTS
        DB_BAD_CREDS
        DB_INSUFFICIENT
        DB_NOT_OWNED
        DB_IO_ERROR
        DB_FULL
        DB_INVALID
    }

    class t_player {
        +t_player_id player_id
        +char username[32]
        +char password_hashed[64]
        +char salt[32]
        +int64 leaderboard_score
        +int64 wallet_points
        +t_item_id current_equipped_character
        +t_item_id current_equipped_theme
        +t_item_id owned_characters[64]
        +t_item_id owned_themes[64]
        +uint32 games_played
        +uint32 games_won
    }
    class t_character {
        +t_item_id character_id
        +char name[32]
        +uint32 abilities
        +int64 cost_points
    }
    class t_theme {
        +t_item_id theme_id
        +char name[32]
        +char description[128]
    }
    class t_rank_entry {
        +t_player_id player_id
        +char username[32]
        +int64 leaderboard_score
    }

    class t_macminidb {
        +pthread_rwlock_t lock
        +t_player_id next_id
    }
    class t_hashmap {
        +t_hm_entry** buckets
        +size_t bucket_count
        +size_t size
    }
    class t_hm_entry {
        +t_player* player
        +t_hm_entry* next
    }
    class t_skiplist {
        +t_skipnode* head
        +int level
        +size_t size
        +uint64 rng
    }
    class t_skipnode {
        +t_player* player
        +int height
        +t_skipnode* forward[]
    }
    class t_catalogue {
        +t_character characters[64]
        +size_t char_count
        +t_theme themes[64]
        +size_t theme_count
    }
    class t_dblog {
        +int fd
        +char path[PATH_MAX]
    }
    class t_flusher {
        +pthread_t thread
        +t_dblog* log
        +pthread_mutex_t lock
        +pthread_cond_t cond
        +int stop
    }
    class t_find_ctx {
        +t_player_id id
        +t_player* match
    }

    class db {
        <<module>> db.c / db_*.c
        +db_open(data_dir, config_dir, out) t_db_result
        +db_close(t_db*)
        +db_signup(...) t_db_result
        +db_login(...) t_db_result
        +db_buy_character / db_buy_theme(...)
        +db_equip_character / db_equip_theme(...)
        +db_record_game(...) t_db_result
        +db_get_player / db_get_character / db_get_theme
        +db_leaderboard / db_rank(...)
        +db_find_by_id / db_persist (internal)
    }
    class hashmap {
        <<module>> hashmap.c / hashmap_ops.c
        +hashmap_create/destroy/foreach
        +hashmap_get / put / hash / find
    }
    class skiplist {
        <<module>> skiplist*.c
        +skiplist_create/destroy
        +skiplist_insert/remove/seek/cmp
        +skiplist_rank/topn/update
        +random_level / node_new
    }
    class catalogue {
        <<module>> catalogue*.c
        +catalogue_load/free
        +catalogue_character/theme
        +catalogue_parse_chars/themes
        +catalogue_open/next_line/split
    }
    class dblog {
        <<module>> log.c / log_append.c / log_replay.c
        +log_open/close/fsync
        +log_append(t_player*)
        +log_replay(cb, ctx)
    }
    class flusher {
        <<module>> flusher.c
        +flusher_start(t_dblog*) t_flusher*
        +flusher_stop(t_flusher*)
    }
    class recovery {
        <<module>> recovery.c
        +recovery_run(log, hm, sl, out_next_id)
    }
    class player_io {
        <<module>> player_write.c / player_read.c
        +player_serialise(t_player*, buf, cap)
        +player_deserialise(buf, len, out)
    }

    t_macminidb *--> t_hashmap : players
    t_macminidb *--> t_skiplist : board
    t_macminidb *--> t_dblog : log
    t_macminidb *--> t_flusher : flusher
    t_macminidb *--> t_catalogue : cat

    t_hashmap *--> "*" t_hm_entry : buckets
    t_hm_entry *--> t_player : owns
    t_skiplist *--> "*" t_skipnode : towers
    t_skipnode o--> t_player : by ref
    t_flusher o--> t_dblog : syncs
    t_catalogue *--> "*" t_character
    t_catalogue *--> "*" t_theme

    db --> t_macminidb : owns rwlock
    db ..> hashmap
    db ..> skiplist
    db ..> catalogue
    db ..> dblog
    db ..> recovery
    db ..> player_io
    hashmap ..> t_hashmap
    skiplist ..> t_skiplist
    skiplist ..> t_rank_entry
    catalogue ..> t_catalogue
    dblog ..> t_dblog
    flusher ..> t_flusher
    recovery ..> dblog
    recovery ..> hashmap
    recovery ..> skiplist
    player_io ..> t_player
```

---

## 3. `libcoreipc` — IPC primitives *(planned — no code yet)*


```mermaid
classDiagram
    direction LR

    class t_ring_buffer {
        +unsigned char* slots
        +size_t record_size
        +size_t capacity
        +size_t head
        +size_t tail
        +pthread_mutex_t mutex
        +atomic_uint_fast64_t drops
    }

    class ring_buffer {
        <<module>> ring_buffer.c
        +ring_init(rb, record_size, capacity) int
        +ring_push(rb, record) int
        +ring_pop(rb, out) int
        +ring_drain(rb, out, max) size_t
        +ring_dropped_count(rb) uint64
        +ring_destroy(rb)
    }
    class unix_socket {
        <<module>> unix_socket.c
        +unixsock_dgram_bind(path, mode) int
        +unixsock_dgram_open(path) int
        +unixsock_dgram_send_nonblock(fd, buf, len) int
        +unixsock_stream_listen(path, backlog, mode) int
        +unixsock_stream_connect(path) int
        +unixsock_send_all / unixsock_recv_all
        +unixsock_set_nonblock(fd)
    }
    class msgqueue {
        <<module>> msgqueue.c
        +msgqueue_open(name, ...) mqd_t
        +msgqueue_send_nonblock(mq, buf, len) int
        +msgqueue_recv_nonblock(mq, buf, len) int
        +msgqueue_close(mq)
        +msgqueue_unlink(name)
    }

    ring_buffer *--> t_ring_buffer : manages

    note for ring_buffer "MPSC bounded ring, non-blocking push, atomic drop counter"
    note for msgqueue "POSIX mq wrappers, O_NONBLOCK drop semantics"
```

---

## 4. `src/tetrish` — secure interactive shell (REPL pipeline)


```mermaid
classDiagram
    direction LR

    class t_token {
        <<enum>>
        END
        RDAPP
        HEREDOC
        RDIN
        RDOUT
        PIPE
        COMMAND
    }

    class t_root {
        +t_history* history
        +t_token_check tkchk[]
        +char* tree_arg_value
        +int stdin_tmp
        +int stdout_tmp
        +t_list* env_list
        +int* pipe
        +struct termios previous
        +struct termios current
        +int heredoc_flag
        +int exit_cmd_flag
        +char* current_dir
    }
    class t_tree {
        +t_token token
        +char* value
        +t_tree* left
        +t_tree* right
    }
    class t_env {
        +char* key
        +char* value
    }
    class t_history {
        +int id
        +char* cmd
        +t_history* next
        +t_history* prev
    }
    class t_expand_var {
        +char* n_cmd
        +char* substring
        +char* dollar_ptr
        +char* start
        +int count
        +int len
    }
    class DaemonInfo {
        +char name[64]
        +pid_t pid
        +char timestamp[128]
    }

    class Init {
        <<module>> 01_init/rc/banner
        +init_root(t_root*, envp) int
        +source_rc / run_rc / set_path
        +print_banner(t_root*)
    }
    class Expander {
        <<module>> 03_expand
        +expand(cmd, env_list) char*
        +single_quote / join_dollar_ptr
        +replace_exit_status
    }
    class Lexer {
        <<module>> 04_lexer
        +lexer(cmd) t_list*
        +count_token / count_char
        +cmd_modifier
    }
    class Parser {
        <<module>> 05_parser
        +parser(lexer, n_token, t_root*) t_tree*
        +tree_node_new(...) t_tree*
    }
    class Executor {
        <<module>> 06_execute
        +recurse_bst(t_tree*, envp, t_root*)
        +find_path / get_exe_path / cmd_join
    }
    class Pipe {
        <<module>> 07_pipe
        +pipe_handler(node, envp, t_root*)
    }
    class Redirection {
        <<module>> 08_redirection
        +rdin/rdout/rdapp/heredoc_handler
        +*_fd(...) int
    }
    class Builtins {
        <<module>> 09_builtin
        +builtin(cmd, t_root*) int
        +echo/cd/pwd/export/unset/env
        +history/exit/usage/help/setenv
    }
    class Signals {
        <<module>> 11_signal
        +shell_ignore_signals
        +child_restore_signals
        +sigint_ignore/restore
    }

    class libft {
        <<static lib>> libft.a
        +ft_split/strdup/strjoin/...
        +ft_lst* linked list
        +get_next_line
    }
    class libcommon {
        <<static lib>> src/common → libcommon.a
        +daemon_spawn / daemon_log
        +resolve_project_root
        +ensure_daemon_files / archive_dir
        +ft_open/close/fork/dup2/pipe/kill
    }
    class SystemPrograms {
        <<binaries>> src/system → bin/*
        +dspawn / dcheck / dkill
        +backup / find / ld / ldr / sys
    }

    t_root *--> t_history : history
    t_root o--> "*" t_env : env_list (t_list)
    t_tree o--> t_tree : left / right (BST)
    t_tree o--> t_token
    t_history o--> t_history : prev / next

    Init ..> t_root
    Expander ..> t_expand_var
    Expander ..> t_root
    Lexer ..> t_token
    Parser ..> t_tree
    Parser ..> t_token
    Executor ..> t_tree
    Executor ..> t_root
    Pipe ..> t_tree
    Redirection ..> t_tree
    Builtins ..> t_root
    Builtins ..> t_env
    Builtins ..> t_history

    Init ..> Expander : REPL pipeline
    Expander ..> Lexer
    Lexer ..> Parser
    Parser ..> Executor
    Executor ..> Pipe
    Executor ..> Redirection
    Executor ..> Builtins

    Init o--> libft
    Init o--> libcommon
    SystemPrograms o--> libcommon
    SystemPrograms o--> libft
    SystemPrograms ..> DaemonInfo
```

---

## 5. `src/tetrisu` — terminal client (notcurses UI)


```mermaid
classDiagram
    direction LR

    class app_state_t {
        <<enum>>
        APP_SPLASH
        APP_MAIN_MENU
        APP_QUIT
    }

    class menu_selection_t {
        +int selected
    }
    class audio_ctx_t {
        +int enabled
        +int music_volume
        +void* music
        +void* menu_move_sfx
        +void* menu_select_sfx
    }
    class render_ctx_t {
        +notcurses* nc
        +ncplane* std
        +ncplane* bg_plane
        +ncplane* menu_plane
        +ncplane* bunny_plane
        +int bg_row / bg_col
        +int bg_rows / bg_cols
        +int cell_px_y / cell_px_x
        +int menu_row / menu_col
        +int bunny_rows / bunny_cols
    }

    class app_state {
        <<module>> app_state.c
        +app_handle_key(current, key) app_state_t
        +menu_move_selection(m, key)
        +menu_item_label(index) char*
        +menu_stub_text(index) char*
    }
    class render_background {
        <<module>> render_background.c
        +render_init(image_path) render_ctx_t
        +render_wait_key(ctx) uint32
        +render_teardown(ctx)
    }
    class render_menu {
        <<module>> render_menu.c
        +render_menu_create(ctx)
        +render_menu_move_bunny(ctx, m)
        +render_menu_show_message(ctx, msg)
    }
    class render_intro {
        <<module>> render_intro.c
        +render_intro_play(ctx, audio, video, audio_path) int
    }
    class audio {
        <<module>> audio.c
        +audio_init(audio) int
        +audio_play_music/once/stop_music
        +audio_load_menu_sfx
        +audio_play_menu_move/select
        +audio_volume_up/down
        +audio_teardown
    }
    class main {
        <<module>> main.c
        +main() : drives state machine
    }

    main o--> render_ctx_t : owns
    main o--> audio_ctx_t : owns
    main o--> menu_selection_t : owns
    main ..> app_state_t : loop state

    main ..> app_state
    main ..> render_background
    main ..> render_menu
    main ..> render_intro
    main ..> audio

    app_state ..> app_state_t
    app_state ..> menu_selection_t
    render_background ..> render_ctx_t
    render_menu ..> render_ctx_t
    render_menu ..> menu_selection_t
    render_intro ..> render_ctx_t
    render_intro ..> audio_ctx_t
    audio ..> audio_ctx_t
```

---

## 6. System-level composition (who links what)

```mermaid
classDiagram
    direction TB

    class libtetrisbrain { <<static lib>> pure game logic }
    class libmacminidb { <<static lib>> player DB + WAL }
    class libcoreipc { <<static lib>> IPC primitives (planned) }
    class libft { <<static lib>> libc }
    class libcommon { <<static lib>> shell helpers }

    class tetrish { <<binary>> interactive shell }
    class tetrisu { <<binary>> terminal client }
    class tetrisd { <<binary, planned>> game server }
    class tetrislogd { <<binary, planned>> logger }
    class tetrisctl { <<binary, planned>> admin CLI }

    tetrish o--> libft
    tetrish o--> libcommon
    tetrisu o--> libtetrisbrain : client prediction
    tetrisd o--> libtetrisbrain
    tetrisd o--> libmacminidb
    tetrisd o--> libcoreipc
    tetrislogd o--> libcoreipc
    tetrisctl o--> libcoreipc
    libcommon o--> libft
```
