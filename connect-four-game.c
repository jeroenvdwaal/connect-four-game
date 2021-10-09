#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>

/* #define PRINT_TREE */

/* Dimensions of the playfield. */
#define GAME_C_ROW 6 /* Number of rows.    */
#define GAME_C_COL 7 /* Number of columns. */

#define GAME_C_FALSE 0
#define GAME_C_TRUE 1

typedef enum
{
    GAME_C_PLAYER_1,
    GAME_C_PLAYER_2,
    GAME_C_EMPTY
} T_GAME_FIELD,
    *PT_GAME_FIELD, T_GAME_PLAYER, *PT_GAME_PLAYER;

/* Error related return codes */
#define GAME_C_OK 0              /* Success */
#define GAME_C_ERR_BAD_COLUMN 1  /* The column number is out of bounds.    */
#define GAME_C_ERR_COLUMN_FULL 2 /* The column was full, chip not inserted */
#define GAME_C_ERR_WINNER 3      /* There is a winner!                     */

typedef struct
{
    char arc_col_chip_count[GAME_C_COL];             /* Number of chips in a particular column */
    T_GAME_FIELD art_fields[GAME_C_COL][GAME_C_ROW]; /* Playfield */
} T_GAME_BOARD, *PT_GAME_BOARD;

typedef struct
{
    unsigned int ui_column;
    unsigned int ui_row;
    T_GAME_PLAYER t_game_played_by;
    unsigned int ui_computer_move;
} T_GAME_UNDO_ITEM, *PT_GAME_UNDO_ITEM;

typedef struct
{
    T_GAME_UNDO_ITEM t_game_undo_items[GAME_C_COL * GAME_C_ROW];
    unsigned int ui_stack_count;
} T_GAME_UNDO_STACK, *PT_GAME_UNDO_STACK;

unsigned game_undo_push(PT_GAME_UNDO_STACK pio_t_game_undo_stack,
                        T_GAME_UNDO_ITEM pi_t_undo_item)
{
    if (pio_t_game_undo_stack->ui_stack_count >= (GAME_C_ROW * GAME_C_COL))
    {
        return GAME_C_FALSE;
    }
    pio_t_game_undo_stack->t_game_undo_items[pio_t_game_undo_stack->ui_stack_count] = pi_t_undo_item;
    pio_t_game_undo_stack->ui_stack_count++;
    return GAME_C_TRUE;
}

/* true or false */
unsigned game_undo_pop(PT_GAME_UNDO_STACK pio_t_game_undo_stack,
                       PT_GAME_UNDO_ITEM po_t_game_undo_item)
{
    if (pio_t_game_undo_stack->ui_stack_count == 0)
    {
        return GAME_C_FALSE;
    }
    pio_t_game_undo_stack->ui_stack_count--;
    *po_t_game_undo_item = pio_t_game_undo_stack->t_game_undo_items[pio_t_game_undo_stack->ui_stack_count];
    return GAME_C_TRUE;
}

unsigned game_undo_peek(PT_GAME_UNDO_STACK pio_t_game_undo_stack,
                        PT_GAME_UNDO_ITEM po_a_game_undo_item)
{
    unsigned ui_index;
    if (pio_t_game_undo_stack->ui_stack_count == 0)
    {
        return GAME_C_FALSE;
    }
    ui_index = pio_t_game_undo_stack->ui_stack_count - 1;
    *po_a_game_undo_item = pio_t_game_undo_stack->t_game_undo_items[ui_index];
    return GAME_C_TRUE;
}

typedef struct
{
    T_GAME_BOARD t_game_board;
    T_GAME_PLAYER t_player_on_move;
    T_GAME_UNDO_STACK t_undo_stack;
    unsigned ui_flag_winner;
} T_GAME, *PT_GAME;

/* proto's */
int opp_look_ahead(T_GAME pi_t_game);

#define M_GAME_COLUMN_FULL(board, column) ((board).arc_col_chip_count[(column)] >= GAME_C_ROW)
#define M_GAME_FAST_INSERT(board, column, player) (board).art_fields[(column)][(board).arc_col_chip_count[(column)]++] = (player);
#define M_GAME_FAST_REMOVE(board, column) (board).art_fields[(column)][--(board).arc_col_chip_count[(column)]] = GAME_C_EMPTY;

void game_board_init(PT_GAME_BOARD pio_a_game_board)
{
    int i_col_count;
    int i_row_count;

    for (i_col_count = 0; i_col_count < GAME_C_COL; i_col_count++)
    {
        pio_a_game_board->arc_col_chip_count[i_col_count] = 0;
        for (i_row_count = 0; i_row_count < GAME_C_ROW; i_row_count++)
        {
            pio_a_game_board->art_fields[i_col_count][i_row_count] = GAME_C_EMPTY;
        }
    }
}

void game_init(PT_GAME pio_a_game, T_GAME_PLAYER pi_t_game_player_first)
{
    game_board_init(&pio_a_game->t_game_board);
    pio_a_game->t_player_on_move = pi_t_game_player_first;

    /* init undo stack */
    pio_a_game->t_undo_stack.ui_stack_count = 0;

    /* win flag to false */
    pio_a_game->ui_flag_winner = GAME_C_FALSE;
}

unsigned int game_winner(PT_GAME pi_a_game, unsigned int pi_ui_column);

unsigned int game_insert_chip_generic(PT_GAME pi_a_game,
                                      unsigned int pi_ui_column,
                                      unsigned *po_a_row,
                                      unsigned int pi_ui_computer_move)
{
    int i_row_count;
    PT_GAME_PLAYER pt_player_on_move = &pi_a_game->t_player_on_move;
    T_GAME_UNDO_ITEM t_game_undo_item;

    if (pi_ui_column >= GAME_C_COL)
    {
        /* this entry is out of bounds */
        return GAME_C_ERR_BAD_COLUMN;
    }

    i_row_count = pi_a_game->t_game_board.arc_col_chip_count[pi_ui_column];
    if (i_row_count >= GAME_C_ROW)
    {
        /* This column is full. */
        return GAME_C_ERR_COLUMN_FULL;
    }

    if (pi_a_game->ui_flag_winner == GAME_C_TRUE)
    {
        /* this game is played out */
        return GAME_C_ERR_WINNER;
    }

    /* insert */
    pi_a_game->t_game_board.art_fields[pi_ui_column][i_row_count] = *pt_player_on_move;
    pi_a_game->t_game_board.arc_col_chip_count[pi_ui_column]++;

    /* push stack information */
    t_game_undo_item.ui_column = pi_ui_column;
    t_game_undo_item.ui_row = pi_a_game->t_game_board.arc_col_chip_count[pi_ui_column] - 1;
    t_game_undo_item.t_game_played_by = *pt_player_on_move;
    t_game_undo_item.ui_computer_move = pi_ui_computer_move;
    assert(game_undo_push(&pi_a_game->t_undo_stack, t_game_undo_item));

    if (po_a_row != NULL)
    {
        *po_a_row = t_game_undo_item.ui_row;
    }

    /* change of player */
    *pt_player_on_move = (*pt_player_on_move == GAME_C_PLAYER_1) ? GAME_C_PLAYER_2 : GAME_C_PLAYER_1;

    pi_a_game->ui_flag_winner = game_winner(pi_a_game, pi_ui_column);
    return GAME_C_OK; /* Everything ok. */
}

unsigned game_insert_chip(PT_GAME pi_a_game, unsigned pi_ui_column, unsigned *pi_a_row)
{
    return game_insert_chip_generic(pi_a_game, pi_ui_column, pi_a_row, GAME_C_FALSE);
}

unsigned int game_undo_chip_insert(PT_GAME pi_a_game, PT_GAME_UNDO_ITEM po_a_game_undo_item)
{
    T_GAME_UNDO_ITEM t_game_undo_item;
    PT_GAME_PLAYER pt_player_on_move = &pi_a_game->t_player_on_move;

    if (game_undo_pop(&pi_a_game->t_undo_stack, &t_game_undo_item) == GAME_C_FALSE)
    {
        return GAME_C_FALSE;
    }

    if (po_a_game_undo_item != NULL)
    {
        *po_a_game_undo_item = t_game_undo_item;
    }

    /* undo the move */
    M_GAME_FAST_REMOVE(pi_a_game->t_game_board, t_game_undo_item.ui_column);

    /* set correct player */
    *pt_player_on_move = (*pt_player_on_move == GAME_C_PLAYER_1) ? GAME_C_PLAYER_2 : GAME_C_PLAYER_1;

    /* reset winner flag */
    pi_a_game->ui_flag_winner = GAME_C_FALSE;
    return GAME_C_TRUE;
}

typedef enum
{
    GAME_C_HORZ,
    GAME_C_VERT,
    GAME_C_DIAG_UP,
    GAME_C_DIAG_DOWN
} T_GAME_DIRECT;

#define M_GAME_MAX(a, b) ((a) > (b)) ? (a) : (b)
#define M_GAME_MIN(a, b) ((a) < (b)) ? (a) : (b)

#define GAME_C_MAX_DIRECTIONS 7

unsigned int game_winner(PT_GAME pi_a_game, unsigned int pi_ui_column)
{
    PT_GAME_BOARD a_board = &pi_a_game->t_game_board;
    unsigned int ui_row = pi_a_game->t_game_board.arc_col_chip_count[pi_ui_column];
    unsigned uir_connect_count[GAME_C_DIAG_DOWN + 1] = {0, 0, 0, 0};
    unsigned int ui_length;
    unsigned int ui_segment;
    unsigned int ui_step;
    unsigned int ui_counter;
    unsigned int ui_done;
    T_GAME_FIELD t_game_field;
    T_GAME_FIELD t_game_temp_field;
    unsigned int ui_line_row;
    unsigned int ui_line_column;

    typedef struct
    {
        int i_dir_column;   /* Line direction for columns. */
        int i_dir_row;      /* Line direction rows. */
        unsigned ui_length; /* Length of the line. */
        unsigned ui_index;  /* Index for chip count. */
    } T_LINE;

    static T_LINE art_lines[GAME_C_MAX_DIRECTIONS] =
        {
            {-1, 1, 0, GAME_C_DIAG_DOWN},
            {-1, 0, 0, GAME_C_HORZ},
            {-1, -1, 0, GAME_C_DIAG_UP},
            {0, -1, 0, GAME_C_VERT},
            {1, -1, 0, GAME_C_DIAG_DOWN},
            {1, 0, 0, GAME_C_HORZ},
            {1, 1, 0, GAME_C_DIAG_UP}};

    /* Input parameters check */
    assert(ui_row > 0);
    assert(pi_a_game != NULL);

    /* Row counter points always to the place to insert to. */
    ui_row--;

    /* determines line lengths. */
    for (ui_counter = 0; ui_counter < GAME_C_MAX_DIRECTIONS; ui_counter++)
    {
        art_lines[ui_counter].ui_length = 0;
    }

    /* segment 0 */
    ui_length = M_GAME_MIN((GAME_C_ROW - 1) - ui_row, pi_ui_column);
    if (ui_length > 0)
    {
        art_lines[0].ui_length = M_GAME_MIN(3, ui_length);
    }

    /* segment 1 */
    ui_length = pi_ui_column;
    if (ui_length > 0)
    {
        art_lines[1].ui_length = M_GAME_MIN(3, ui_length);
    }

    /* segment 2 */
    ui_length = M_GAME_MIN(pi_ui_column, ui_row);
    if (ui_length > 0)
    {
        art_lines[2].ui_length = M_GAME_MIN(3, ui_length);
    }

    /* segment 3 */
    ui_length = ui_row;
    if (ui_length > 2)
    {
        art_lines[3].ui_length = M_GAME_MIN(3, ui_length);
    }

    /* segment 4 */
    ui_length = M_GAME_MIN(ui_row, (GAME_C_COL - 1) - pi_ui_column);
    if (ui_length > 0)
    {
        art_lines[4].ui_length = M_GAME_MIN(3, ui_length);
    }

    /* segment 5 */
    ui_length = (GAME_C_COL - 1) - pi_ui_column;
    if (ui_length > 0)
    {
        art_lines[5].ui_length = M_GAME_MIN(3, ui_length);
    }

    /* segment 6 */
    ui_length = M_GAME_MIN((GAME_C_ROW - 1) - ui_row, (GAME_C_COL - 1) - pi_ui_column);
    if (ui_length > 0)
    {
        art_lines[6].ui_length = M_GAME_MIN(3, ui_length);
    }

    t_game_field = a_board->art_fields[pi_ui_column][ui_row];
    for (ui_segment = 0; ui_segment < GAME_C_MAX_DIRECTIONS; ui_segment++)
    {
        /* start point for traveling along a segment line */
        ui_line_column = pi_ui_column + art_lines[ui_segment].i_dir_column;
        ui_line_row = ui_row + art_lines[ui_segment].i_dir_row;
        ui_done = 0;

        /* step along the line */
        for (ui_step = 0; ui_step < art_lines[ui_segment].ui_length && !ui_done; ui_step++)
        {
            t_game_temp_field = a_board->art_fields[ui_line_column][ui_line_row];
            if (t_game_temp_field == t_game_field)
            {

                uir_connect_count[art_lines[ui_segment].ui_index]++;
                if (uir_connect_count[art_lines[ui_segment].ui_index] >= 3)
                {
                    /* Four on a row detected. */
                    return GAME_C_TRUE;
                }
            }
            else
            {
                ui_done = 1;
            }
            ui_line_column += art_lines[ui_segment].i_dir_column;
            ui_line_row += art_lines[ui_segment].i_dir_row;
        }
    }
    return GAME_C_FALSE;
}

unsigned game_won(PT_GAME pi_a_game, PT_GAME_PLAYER po_t_player,
                  unsigned *po_ui_computer_move)
{
    T_GAME_UNDO_ITEM t_game_undo_item;

    if (pi_a_game->ui_flag_winner == GAME_C_FALSE)
    {
        return GAME_C_FALSE;
    }

    assert(game_undo_peek(&pi_a_game->t_undo_stack, &t_game_undo_item));

    if (po_t_player != NULL)
    {
        *po_t_player = t_game_undo_item.t_game_played_by;
    }

    if (po_ui_computer_move != NULL)
    {
        *po_ui_computer_move = t_game_undo_item.ui_computer_move;
    }
    return GAME_C_TRUE;
}

/**** new file, opponent.c  ****/

/* this module is a computer opponent for the connect4 game. */

#define OPP_C_CHECKSIZE 4
#define OPP_C_MAX_LOOK_AHEAD_DEPTH 10

/* module errors */
#define OPP_C_OK 0
#define OPP_C_INV_MAX_DEPTH 1
/* end module errors */

static int ari_score_table[OPP_C_CHECKSIZE + 1] = {0, 1, 3, 8, 200};
unsigned int opp_gui_look_ahead_depth = 1;

int opp_set_look_ahead_depth(unsigned int pi_ui_look_ahead_depth)
{
    if (pi_ui_look_ahead_depth <= OPP_C_MAX_LOOK_AHEAD_DEPTH)
    {
        opp_gui_look_ahead_depth = pi_ui_look_ahead_depth;
    }
    else
    {
        return OPP_C_INV_MAX_DEPTH;
    }
    return OPP_C_OK;
}

/* For use with the opp_check_segment() function. */
typedef enum
{
    OPP_C_SEGM_HORZ,
    OPP_C_SEGM_VERT,
    OPP_C_SEGM_DIAG_UP,
    OPP_C_SEGM_DIAG_DOWN
} T_OPP_SEGMENT;

int opp_check_segment(PT_GAME_BOARD pi_a_game_board, unsigned int pi_ui_column,
                      unsigned int pi_ui_row, T_OPP_SEGMENT pi_t_opp_segment)

{
    unsigned int ui_column_count;
    unsigned int ui_occurrence_count[] = {0, 0, 0};
    int i_add_column = 0;
    int i_add_row = 0;

    assert(pi_a_game_board != NULL);
    assert(pi_ui_column < GAME_C_COL);
    assert(pi_ui_row < GAME_C_ROW);

    switch (pi_t_opp_segment)
    {
    case OPP_C_SEGM_HORZ:
        i_add_column = 1;
        assert((pi_ui_column + OPP_C_CHECKSIZE) <= GAME_C_COL);
        break;

    case OPP_C_SEGM_VERT:
        i_add_row = 1;
        assert((pi_ui_row + OPP_C_CHECKSIZE) <= GAME_C_ROW);
        break;

    case OPP_C_SEGM_DIAG_UP:
        i_add_column = 1;
        i_add_row = 1;
        assert((pi_ui_column + OPP_C_CHECKSIZE) <= GAME_C_COL);
        assert((pi_ui_row + OPP_C_CHECKSIZE) <= GAME_C_ROW);
        break;

    case OPP_C_SEGM_DIAG_DOWN:
        i_add_column = 1;
        i_add_row = -1;
        assert((pi_ui_column + OPP_C_CHECKSIZE) <= GAME_C_COL);
        assert(pi_ui_row >= (OPP_C_CHECKSIZE - 1) &&
               pi_ui_row < GAME_C_ROW);
        break;

    default:
        /* Invalid parameter */
        assert(0);
        break;
    }

    /* Determines the occurrences of player chips in this segment. */
    for (ui_column_count = 0; ui_column_count < OPP_C_CHECKSIZE;
         ui_column_count++)
    {
        ui_occurrence_count[pi_a_game_board->art_fields[pi_ui_column][pi_ui_row]]++;
        pi_ui_column += i_add_column;
        pi_ui_row += i_add_row;
    }

    /* 
  ** When the segment is empty, there is no contribution to one of the 
  ** players score.
  */

    if (ui_occurrence_count[GAME_C_EMPTY] == OPP_C_CHECKSIZE)
        return 0;

    /*  
  **  if both players have one or more coins in this segment, 
  **  it is never possible to achieve 4 on a row here. Therefore
  **  the score is zero.
  */
    if (ui_occurrence_count[GAME_C_PLAYER_1] != 0 &&
        ui_occurrence_count[GAME_C_PLAYER_2] != 0)
        return 0;

    /*
  ** One of the players has a score contribution because of this segment.
  */
    if (ui_occurrence_count[GAME_C_PLAYER_1])
    {

        return ari_score_table[ui_occurrence_count[GAME_C_PLAYER_1]];
    }
    else
    {
        return -ari_score_table[ui_occurrence_count[GAME_C_PLAYER_2]];
    }
}

/* calculate which player has the best opportunities */
int opp_game_evaluation(PT_GAME_BOARD pi_a_game_board)
{
    unsigned int ui_column_count;
    unsigned int i_row_count;
    int i_grand_total = 0;

    /* Checking all possible horizontal segments. */
    for (i_row_count = 0; i_row_count < GAME_C_ROW; i_row_count++)
    {
        for (ui_column_count = 0;
             ui_column_count <= (GAME_C_COL - OPP_C_CHECKSIZE); ui_column_count++)
        {
            i_grand_total += opp_check_segment(pi_a_game_board, ui_column_count,
                                               i_row_count, OPP_C_SEGM_HORZ);
        }
    }

    /* Checking all possible vertical segments. */
    for (ui_column_count = 0; ui_column_count < GAME_C_COL; ui_column_count++)
    {
        for (i_row_count = 0;
             i_row_count <= (GAME_C_ROW - OPP_C_CHECKSIZE); i_row_count++)
        {
            i_grand_total += opp_check_segment(pi_a_game_board, ui_column_count,
                                               i_row_count, OPP_C_SEGM_VERT);
        }
    }

    /* 
  ** Checking all possible diagonal segments 
  ** (from bottom left to upper right). 
  */
    for (ui_column_count = 0;
         ui_column_count <= (GAME_C_COL - OPP_C_CHECKSIZE); ui_column_count++)
    {
        for (i_row_count = 0;
             i_row_count <= (GAME_C_ROW - OPP_C_CHECKSIZE); i_row_count++)
        {
            i_grand_total += opp_check_segment(pi_a_game_board, ui_column_count,
                                               i_row_count, OPP_C_SEGM_DIAG_UP);

            i_grand_total += opp_check_segment(pi_a_game_board, ui_column_count,
                                               i_row_count + 3, OPP_C_SEGM_DIAG_DOWN);
        }
    }

    return i_grand_total;
}

#define OPP_C_MAXIMIZE 0
#define OPP_C_MINIMIZE 1
#define OPP_C_FALSE 0
#define OPP_C_TRUE 1

unsigned opp_do_computer_move(PT_GAME pi_a_game, unsigned *po_ui_column,
                              unsigned *po_ui_row)
{
    unsigned ui_column;

    ui_column = opp_look_ahead(*pi_a_game);
    game_insert_chip_generic(pi_a_game, ui_column, po_ui_row, GAME_C_TRUE);

    if (po_ui_column != NULL)
    {
        *po_ui_column = ui_column;
    }
    return GAME_C_TRUE;
}

int opp_look_ahead(T_GAME pi_t_game)

{
    unsigned int ui_depth = 0; /* Actual think ahead depth. */
    int i_counter;
    int i_temp;

    int ir_eval_value[OPP_C_MAX_LOOK_AHEAD_DEPTH];            /* Values as result of the board evaluation. */
    unsigned int uir_mini_max[OPP_C_MAX_LOOK_AHEAD_DEPTH];    /* LUT gives min/max operation per level. */
    unsigned int uir_chip_played[OPP_C_MAX_LOOK_AHEAD_DEPTH]; /* Per depth the last chip inserted */
    unsigned int uir_is_leaf[OPP_C_MAX_LOOK_AHEAD_DEPTH];     /* Leaf or not. */
    unsigned int uir_recom_move[OPP_C_MAX_LOOK_AHEAD_DEPTH];  /* Recommended move to play */
    T_GAME_PLAYER t_player_on_move;
    unsigned int ui_winner = GAME_C_FALSE;

    for (i_counter = 0; i_counter < OPP_C_MAX_LOOK_AHEAD_DEPTH; i_counter++)
    {
        uir_chip_played[i_counter] = 0;
    }

    /* Prepare LUT for maximize/minimize operation per depth level */
    t_player_on_move = pi_t_game.t_player_on_move;
    for (i_counter = 0; i_counter < OPP_C_MAX_LOOK_AHEAD_DEPTH; i_counter++)
    {
        if (t_player_on_move == GAME_C_PLAYER_1)
        {
            uir_mini_max[i_counter] = OPP_C_MAXIMIZE;
            ir_eval_value[i_counter] = INT_MIN;
        }
        else
        {
            uir_mini_max[i_counter] = OPP_C_MINIMIZE;
            ir_eval_value[i_counter] = INT_MAX;
        }
        t_player_on_move = (t_player_on_move == GAME_C_PLAYER_1) ? GAME_C_PLAYER_2 : GAME_C_PLAYER_1;
        uir_recom_move[i_counter] = 0;
    }

    /* Start of game tree evaluation. */
    do
    {
        if (ui_depth < opp_gui_look_ahead_depth &&
            uir_chip_played[ui_depth] < GAME_C_COL && !ui_winner)
        {
            if (M_GAME_COLUMN_FULL(pi_t_game.t_game_board, uir_chip_played[ui_depth]))
            {
                uir_chip_played[ui_depth]++;
            }
            else
            {
                /* Insert chip */
                t_player_on_move = (uir_mini_max[ui_depth] == OPP_C_MAXIMIZE) ? GAME_C_PLAYER_1 : GAME_C_PLAYER_2;
                M_GAME_FAST_INSERT(pi_t_game.t_game_board, uir_chip_played[ui_depth], t_player_on_move);
#ifdef PRINT_TREE
                printf("\n");
                for (i_counter = 0; i_counter < ui_depth; i_counter++)
                    printf("  ");
                printf("I(c%u, d%u)", uir_chip_played[ui_depth], ui_depth);
#endif
                uir_is_leaf[ui_depth] = OPP_C_FALSE;
                ui_winner = game_winner(&pi_t_game, uir_chip_played[ui_depth]);
                ui_depth++;
                uir_is_leaf[ui_depth] = OPP_C_TRUE;
                if (uir_mini_max[ui_depth] == OPP_C_MAXIMIZE)
                {
                    ir_eval_value[ui_depth] = INT_MIN;
                }
                else
                {
                    ir_eval_value[ui_depth] = INT_MAX;
                }
            }
        }
        else
        {
            if (uir_chip_played[ui_depth] >= GAME_C_COL)
            {
                uir_chip_played[ui_depth] = 0;
            }
            ui_depth--;

            if (uir_is_leaf[ui_depth + 1])
            {
                i_temp = opp_game_evaluation(&pi_t_game.t_game_board);
            }
            else
            {
                i_temp = ir_eval_value[ui_depth + 1];
            }

            if (uir_mini_max[ui_depth] == OPP_C_MAXIMIZE)
            {
                if (i_temp > ir_eval_value[ui_depth])
                {
                    ir_eval_value[ui_depth] = i_temp;
                    uir_recom_move[ui_depth] = uir_chip_played[ui_depth];
                }
            }
            else
            {
                if (i_temp < ir_eval_value[ui_depth])
                {
                    ir_eval_value[ui_depth] = i_temp;
                    uir_recom_move[ui_depth] = uir_chip_played[ui_depth];
                }
            }
            /* remove chip */
            M_GAME_FAST_REMOVE(pi_t_game.t_game_board, uir_chip_played[ui_depth]);
            ui_winner = GAME_C_FALSE;
#ifdef PRINT_TREE
            printf("\n");
            for (i_counter = 0; i_counter < ui_depth; i_counter++)
                printf("  ");
            printf("R(c%u,d%u,v%d)", uir_chip_played[ui_depth], ui_depth, i_temp);
#endif
            uir_chip_played[ui_depth]++;
        }
    } while (uir_chip_played[0] < GAME_C_COL);
    printf("\nRecommended move %u, value %d\n", uir_recom_move[0], ir_eval_value[0]);
    return uir_recom_move[0];
}
/**** end file, opponent.c ****/

/** SCRATCH FUNCTIONS **/

void game_print(const PT_GAME pi_a_game)
{
    int i_row_count, i_col_count;
    printf("Player %d on the move.\n", pi_a_game->t_player_on_move);
    for (i_row_count = (GAME_C_ROW - 1); i_row_count >= 0; i_row_count--)
    {
        printf("%1d: ", i_row_count);
        for (i_col_count = 0; i_col_count < GAME_C_COL; i_col_count++)
        {
            switch (pi_a_game->t_game_board.art_fields[i_col_count][i_row_count])
            {
            case GAME_C_PLAYER_1:
                putchar('O');
                break;

            case GAME_C_PLAYER_2:
                putchar('X');
                break;

            case GAME_C_EMPTY:
                putchar('-');
                break;

            default:
                putchar('?');
                break;
            }
            putchar(' ');
        }
        putchar('\n');
    }
    putchar('\n');
}

void main(void)
{
    T_GAME t_game;
    char c_done = 0;
    char arc_scan[3];
    int i_column, i_return;
    int i_player;
    unsigned ui_computer;

    game_init(&t_game, GAME_C_PLAYER_1);
    opp_set_look_ahead_depth(6);

    game_print(&t_game);
    while (!c_done)
    {
        puts("Enter column number, q to quit or t for take back:");
        scanf("%1s", arc_scan);
        if (arc_scan[0] == 'q')
        {
            c_done = 1;
        }
        else if (arc_scan[0] == 't')
        {
            game_undo_chip_insert(&t_game, NULL);
            game_print(&t_game);
        }
        else
        {
            i_column = atoi(arc_scan);
            i_return = game_insert_chip(&t_game, i_column, NULL);
            game_print(&t_game);
            if (game_won(&t_game, &i_player, &ui_computer))
            {
                printf("The game is over. Player %d won. Played by:%u\n", i_player, ui_computer);
            }
            else
            {
                opp_do_computer_move(&t_game, NULL, NULL);
                if (game_won(&t_game, &i_player, &ui_computer))
                {
                    printf("The game is over. Player %d won. Played by:%u\n", i_player, ui_computer);
                }
                game_print(&t_game);
            }
        }
    }
}