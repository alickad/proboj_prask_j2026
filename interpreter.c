#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>

// Source - https://stackoverflow.com/q/9230554
// Edit: what? this is not relevant at all. ??
#if defined(_WIN32) || defined(WIN32)

#define PATH_SEPARATOR '\\'
#include <io.h>
#define F_OK 0
#define access _access

#else

#define PATH_SEPARATOR '/'
#include <unistd.h>

#endif

#define MAX_UNGET_CHARS 128


#define MAX_BYTECODE_LENGTH 16384
/* premenne v programe + "systemova" premenna, to co hrac zahral */
#define MEMORY_SIZE 64
/* maximalny pocet premennych, ktore si mozes v programe definovat */
#define MAX_VARIABLE_COUNT 63
/* maximalna dlzka nazvu premennej, vratane null terminatora */
#define MAX_VARIABLE_LENGTH 64
#define STACK_SIZE 64
#define MAX_NESTED_IFS 16

/* najdlhsia dlzka riadku pri ktorej sa zobrazuje error, ktory vypise riadok a ukaze na chybu */
/*                                                                                      ^^^^^ */
#define MAX_LINE_LENGHT_TO_SHOW_ERROR_HELP 128

/* Najvacsia povolena dlzka output file nazvu */
#define MAX_OUTPUT_FILE_NAME_LENGHT 256

/* Kolko miesta potrebujeme na nazvy vsetkych premennych,
	teda MAX_VARIABLE_COUNT * MAX_VARIABLE_LENGTH */
#define VARIABLE_NAME_BUFFER_SIZE 4096

/* Kde v pamati je premenna, co zahral oponent naposledy */
#define ADDRESS_OPPONENTS_LAST_PLAY 63

/* Velkost stacku operatorov, co pouzivame pri parsovani.
	Malo by to byt STACK_SIZE * 2. */
#define MAX_OPERATOR_STACK_SIZE 128
/* TODO: checkovat hlbku stacku pocas parsovania, aby sme pri runtime mali guarantee ze neoverflowneme */

/* Kody bytecodovych instrukcii.
	Niektore instrukcie maju za sebou parameter. Niekedy parameter moze
	potrebovat aj viac bytov, takze su rozne verzie instrukcie podla toho,
	aky dlhy parameter maju.
	Viac bytove parametre su little endian, teda na zaciatku je najvacsia cislica. */

/* PLAY: zahraj tah a skonci program.
	To ze PLAY_ROCK je 0 znamena, ze ked sa nerozhodnes hrat nic,
	tak defaultne zahras kamen, kedze program defaultne konci nulami.
	TODO: to momentalne neni pravda, lebo ten buffer s programom je uninitialized,
	ale aj tak to znie ako blbost a mozno by sa to mohlo vyriesit aj lepsie,
	napriklad enforcenut aby sa zahralo nieco aj vonku z ifu? */
#define INST_PLAY_ROCK        0
#define INST_PLAY_PAPER       1
#define INST_PLAY_SCISSORS    2
/* PUSH: potom nasleduje cislo, ktore chceme pushnut na stack */
#define INST_PUSH             3
#define INST_PUSH_2           4
#define INST_PUSH_4           5
/* LOAD, STORE: potom nasleduje index v pamati, z ktoreho chceme nacitat
	cislo na stack/do ktoreho chceme ulozit cislo zo stacku. */
#define INST_LOAD             6
#define INST_STORE            7
/* aritmetika, porovnavanie, logicke operacie: zoberieme dve cisla zo stacku,
	spravime operaciu a pushneme vysledok. MINUS je unarne minus. */
#define INST_ADD              8
#define INST_SUBTRACT         9
#define INST_UNARY_MINUS     10
#define INST_MULTIPLY        11
#define INST_DIVIDE          12
#define INST_MODULO          13
#define INST_EQUAL           14
#define INST_LESS_THAN       15
#define INST_LESS_EQUAL      16
#define INST_GREATER_THAN    17
#define INST_GREATER_EQUAL   18
#define INST_OR              19
#define INST_AND             20
#define INST_NOT             21
#define INST_XOR             22
/* JUMP_IF_ZERO: zoberieme cislo zo stacku a ak je to 0, tak skocime na take miesto
v programe, ako hovori parameter. Je iba verzia s 2 parametrami, lebo ked piseme
tu instrukciu, tak nevieme dopredu, ako daleko bude ten if koncit. */
#define INST_JUMP_IF_ZERO_2  23
#define INST_DEBUG_ASSERT    125
#define INST_DEBUG_PRINT     126

/* NULL: defaultná hodnota bytecode bufferu, neicializovaná hodnota, keď ju prečítame vieme ze program skončil */
#define INST_NULL            127

/* Kody aritmetickych operacii, co davame na stack.
	Musia byt zoradene podla prednosti (napriklad `+` ma prednost pred `*`),
	aby sa potom dali porovnavat.
	TODO: popisat jak presne funguje prednost, pri veciach jak zatvorky a unarne minus
	Pointa je ze veci mozu mat inu prednost ze koho mozu oni vyhodit zo stacku
	a ze kto moze vyhodit zo stacku ich.
	Begin group je zaciatok zatvorky alebo zaciatok celeho vyrazu*/

#define OP_BEGIN_GROUP   0
#define OP_OR            1
#define OP_AND           2
#define OP_NOT           3
#define OP_XOR           4
#define OP_EQUAL         5
#define OP_NOT_EQUAL     6
#define OP_LESS_THAN     7
#define OP_LESS_EQUAL    8
#define OP_GREATER_THAN  9
#define OP_GREATER_EQUAL 10
#define OP_ADD           11
#define OP_SUBTRACT      12
#define OP_UNARY_MINUS   13
#define OP_MULTIPLY      14
#define OP_DIVIDE        15
#define OP_MODULO        16

#define PRECEDENCE_LOGICAL_OP 1
#define PRECEDENCE_COMPARE 5
#define PRECEDENCE_ADD_SUBTRACT 11
#define PRECEDENCE_MULTIPLY_DIVIDE 13

#define TURN_ROCK 0
#define TURN_PAPER 1
#define TURN_SCISSORS 2
#define TURN_ERROR 3
#define TURN_ASSERT_FAILED 4

#define VARIABLE_UNINITIALIZED -6767

typedef unsigned char u_char;

typedef struct {
	FILE *stream;
	int line_number;
	int prev_line_character_number;
	int character_number;
	char unget_buffer[MAX_UNGET_CHARS];
	int unget_buffer_size;
} ProgramReader;

// This function was generated by Gemini Flash 3.1
void ProgramReader_init(ProgramReader* reader, FILE *stream) {
    reader->stream = stream;
    reader->line_number = 1;
    reader->character_number = 1;
    reader->unget_buffer_size = 0;
}
			
int ProgramReader_fgetc(ProgramReader* reader) {
	char c;
	if (reader->unget_buffer_size > 0) {
		reader->unget_buffer_size--;
		c = reader->unget_buffer[reader->unget_buffer_size];
		// #ifdef DEEP_DEBUG
		// 	if (c == '\n')
		// 		printf("[DEEP DEBUG] Reading from unget buffer: `\\n`\n");
		// 	else
		// 		printf("[DEEP DEBUG] Reading from unget buffer: `%c`\n", c);
		// #endif
	} else {
		int char_ = fgetc(reader->stream);
		if (char_ == EOF) return EOF;
		c = char_;
	}

	if (c == '\n') {
		reader->line_number++;
		reader->prev_line_character_number = reader->character_number;
		reader->character_number = 1;
	} else {
		reader->character_number++;
	}

	return c;
}

int ProgramReader_ungetc(char c, ProgramReader* reader) {
	if (reader->unget_buffer_size >= MAX_UNGET_CHARS) {
		printf("Fatal error: unget_buffer is full.");
		// TODO: handle this in every function call in the script
		return EOF;
	}

	reader->unget_buffer[reader->unget_buffer_size] = c;
	reader->unget_buffer_size++;

	
	if (c == '\n') {
		reader->line_number--;
		reader->character_number = reader->prev_line_character_number;
	} else {
		reader->character_number--;
	}
	// #ifdef DEEP_DEBUG
	// 	if (c == '\n')
	// 		printf("[DEEP DEBUG] Writing to unget buffer: `\\n` (size after write: %i) char #%i\n", reader->unget_buffer_size, reader->character_number);
	// 	else
	// 		printf("[DEEP DEBUG] Writing to unget buffer: `%c` (size after write: %i) char #%i\n", c, reader->unget_buffer_size, reader->character_number);
	// #endif
	return c;
}

void ProgramReader_move_discardchars(ProgramReader* reader, long offset) {
	for (int i = 0; i < offset; i++) 
		ProgramReader_fgetc(reader);
}

typedef struct {
	int length;
	unsigned char *buffer;
} BytecodeWriter;

typedef struct {
	int top;
	int buffer[MAX_OPERATOR_STACK_SIZE];
} OperatorStack;

#define IS_WHITESPACE(ch) ((ch) == ' ' || (ch) == '\t' || (ch) == '\r')


/* Ako whitespace sa nerataju nove riadky, tie sa riesia zvlast */
void read_whitespace(ProgramReader *reader) {
	int c;
	do {
		c = ProgramReader_fgetc(reader);
		if (c == EOF) return;
	} while (IS_WHITESPACE(c));
	ProgramReader_ungetc(c, reader);
}

/* Nacita zo vstupu slovo, ktore zacina na pismeno a pokracuje
	pismenami, cislami a podciarkovnikmi, a zapise ho do `dest`.
	Ak sa podari, vrati 1. Ak na vstupe neni slovo, tak neprecita nic a vrati 0.

	Ako kazda funkcia na citanie vstupu, ak je uspesna, tak precita aj
	nasledujuci whitespace. */
int read_word(ProgramReader *reader, char *dest) {
	int success;
	int i;
	int c;
	success = 1;
	c = ProgramReader_fgetc(reader);
	if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')) {
		success = 0;
		goto end;
	}
	for (i = 0; i < MAX_VARIABLE_LENGTH; i++) {
		*dest = c;
		dest++;
		c = ProgramReader_fgetc(reader);
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) goto end;
	}
	/* Ak sme sa dostali sem, tak mame na vstupe slovo, co je moc dlhe */
	/* TODO: compile error: moc dlhe slovo */
	/* Precitali sme znak, ktory uz nie je sucast slova, alebo sme na konci vstupu */
	end:
	if (c != EOF) {
		ProgramReader_ungetc(c, reader);
	}
	*dest = '\0';
	read_whitespace(reader);
	return success;
}

#if 0
/* Ak je na vstupe slovo `keyword`, tak ho precita a vrati 1.
	Inak necha vstup tak a vrati 0.

	Ako kazda funkcia na citanie vstupu, ak je uspesna, tak precita aj
	nasledujuci whitespace.

	TODO: tu je niekde chyba a treba fixnut */
int read_keyword(ProgramReader *reader, char *keyword) {
	/* Ak zacneme citat a potom zistime, ze to neni to spravne slovo,
		tak sa budeme chciet vratit tam kde sme zacali. */
	fpos_t starting_pos;
	char c; /* znak co citame */

	c = fgetc(reader->stream);
	printf("reading keyword \"%s\", first char will be '%c' (code %d)\n", keyword, c, c);
	ungetc(c, reader->stream);

	//printf("read_keyword: getting file position...\n"); fflush(stdout);
	fgetpos(reader->stream, &starting_pos);
	/* BUG: z nejakeho dovodu toto nezachovava poziciu a zmaze newline co nasleduje */
	fsetpos(reader->stream, &starting_pos);
	//printf("read_keyword: got file position.\n"); fflush(stdout);

	c = fgetc(reader->stream);
	printf("got file position, first char will be '%c' (code %d)\n", c, c);
	ungetc(c, reader->stream);

	while (*keyword != '\0') {
		c = fgetc(reader->stream);
		printf("read character '%c' (code %d)\n", c, c);
		if (c != *keyword) goto fail;
		keyword++;
	}
	/* Ak sme precitali cele keyword, este skontrolujeme, ci tam to slovo aj naozaj konci. */
	c = fgetc(reader->stream);
	printf("success, read character '%c' (code %d)\n", c, c);
	if (c >= 'a' && c <= 'z' || (c >= 'A' && c <= 'Z')) goto fail;
	/* Precitali sme aj znak za slovom, takze ho vratime spat. */
	ungetc(c, reader->stream);
	read_whitespace(reader);
	return 1;

	fail:
	fsetpos(reader->stream, &starting_pos);

	c = fgetc(reader->stream);
	printf("fail, next char will be '%c' (code %d)\n", c, c);
	ungetc(c, reader->stream);

	return 0;
}
#endif

/* Ak je na vstupe znak `target`, precita ho a vrati 1, inak necha vstup tak a vrati 0.
	Ako kazda funkcia na citanie vstupu, ak je uspesna, tak precita aj nasledujuci whitespace. */
int read_char(ProgramReader *reader, char target) {
	int c;
	c = ProgramReader_fgetc(reader);
	if (c == EOF) return 0;
	if (c == target) {
		read_whitespace(reader);
		return 1;
	}
	ProgramReader_ungetc(c,reader);
	return 0;
}

int read_string(ProgramReader *reader, const char* str) {
	int i;
	int c;
	for (i = 0; str[i] != '\0'; i++) {
		c = ProgramReader_fgetc(reader);
		if ((u_char)c != str[i]) {
			if (c != EOF) {
				ProgramReader_ungetc((u_char)c, reader);
			};
			for (i--; i >= 0; i--) {
				ProgramReader_ungetc(str[i], reader);
			};
			return 0;
		}
	}
	read_whitespace(reader);
	return 1;
}

int read_keyword(ProgramReader *reader, char *keyword) {
	int i;
	int c;
	for (i = 0; keyword[i] != '\0'; i++) {
		c = ProgramReader_fgetc(reader);
		if ((u_char)c != keyword[i]) {
			if (c != EOF) {
				ProgramReader_ungetc((u_char)c, reader);
			};
			for (i--; i >= 0; i--) {
				ProgramReader_ungetc(keyword[i], reader);
			};
			return 0;
		}
	}
	/* Ak sa cele slovo zhoduje s keyword, este skontrolujeme,
		ci nahodou to slovo nepokracuje dalej, kedy by sa to neratalo */
	c = ProgramReader_fgetc(reader);
	if (c != EOF) {
		ProgramReader_ungetc((u_char)c, reader);
	}
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
		for (i--; i >= 0; i--) {
			ProgramReader_ungetc(keyword[i], reader);
		};
		return 0;
	}
	read_whitespace(reader);
	return 1;
}

/* Ak je na vstupe koniec riadku, a pred nim mozno whitespace a komentar,
	tak precita vsetok whitespace aj nove riadky, az kym nenarazi na naozajstny
	zaciatok nejakeho riadku alebo koniec suboru.
	Vrati 1 ak bol na vstupe koniec riadku, inak 0.

	TODO: fixnut */
int read_new_line(ProgramReader *reader) {
	int c;
	int success;
	success = 0;
	read_whitespace(reader);
	while (1) {
		c = ProgramReader_fgetc(reader);
		/* ak vidime komentar, tak citame az do konca riadku */
		if ((u_char)c == '#') {
			do {
				c = ProgramReader_fgetc(reader);
			} while ((u_char)c != '\n' && c != EOF);
			success = 1;
		}
		/* ak sme naposledy precitali novy riadok, tak si pamatame ze ho mame */
		if ((u_char)c == '\n') {
			success = 1;
			read_whitespace(reader);
		}
		/* ak tam je iny znak alebo koniec vstupu, tak skoncime */
		else {
			if (c != EOF) {
				ProgramReader_ungetc((u_char)c, reader);
			};
			return success || c == EOF;
		}
	}
}

/* TODO: pridat tam aj nazov programu */
void compile_error(ProgramReader *reader, char *message, bool show_help_message) {
	printf("Chyba pri kompilacii, riadok %d, znak %d: %s\n", reader->line_number, reader->character_number, message);
	if (show_help_message) {
		rewind(reader->stream);
		int c = 'a';
		int curr_line = 1;
		while (c != EOF && curr_line < reader->line_number) {
			if ((u_char)c == '\n') curr_line++;
			c = fgetc(reader->stream);
		}
		while ((u_char)c != '\n' && c != EOF) {
			printf("%c", (u_char)c);
			c = fgetc(reader->stream);
		}
		printf("\n");
		for (int i = 0; i < reader->character_number-3; i++) printf(" ");
		printf("^\n");
	}
	exit(EXIT_FAILURE);
}

/* Ak je na vstupe cislo (kladne cele), tak ho precita a ulozi do `dest` a vrati 1.
	Inak vstup necha tak a vrati 0.

	Ako kazda funkcia na citanie vstupu, ak je uspesna, tak precita aj nasledujuci whitespace.

	TODO: overflow check a tiez check ze nenasleduju rovno za cislicami pismena */
int read_number(ProgramReader *reader, int *dest) {
	int success;
	int c;
	success = 1;
	c = ProgramReader_fgetc(reader);
	if (c == EOF) return 0;
	if (c < '0' || c > '9') {
		success = 0;
		*dest = -1;
		goto end;
	}
	*dest = 0;
	do {
		*dest *= 10;
		*dest += c - '0';
		c = ProgramReader_fgetc(reader);
	} while (c >= '0' && c <= '9');
	end:
	if (c != EOF) ProgramReader_ungetc((u_char)c, reader);
	read_whitespace(reader);
	return success;
}

void write_instruction(BytecodeWriter *writer, unsigned char inst) {
	/* TODO: checknut ci nemame moc dlhy program */
	writer->buffer[writer->length] = inst;
	writer->length++;
}

int variable_id_by_name(ProgramReader *reader, char *variable_names, char *name) {
	int i;
	for (i = 0; i < MAX_VARIABLE_COUNT; i++) {
		if (strcmp(&variable_names[i * MAX_VARIABLE_LENGTH], name) == 0) return i;
	}
	#ifdef DEBUG
		printf("Unknown variable name: %s\n", name);
	#endif
	compile_error(reader, "neznamy nazov premennej", true);
	return -1;
}

/* TODO: pocitat si kolko by sme na to potrebovali stack spaceu pocas runtimeu,
	aby sme vedeli garantovat ze ked sa to skompiluje tak to bude ok. */
int operator_stack_push(OperatorStack *stack, int item) {
	#ifdef DEEP_DEBUG
		printf("[DEEP DEBUG] operator_stack_push(): pushing %i\n", item);
	#endif
	stack->top++;
	if (stack->top >= MAX_OPERATOR_STACK_SIZE)
		return EOF;
	stack->buffer[stack->top] = item;
	return stack->top;
}

void operator_stack_pop_until(OperatorStack *stack, BytecodeWriter *writer, int precedence) {
	#ifdef DEEP_DEBUG
		int top_before = stack->top;
		printf("[DEEP DEBUG] operator_stack_pop_until(): stack top: %i, precedence: %i\n", stack->top, precedence);
	#endif
	while (stack->buffer[stack->top] >= precedence) {
		#ifdef DEEP_DEBUG
			printf("[stack_top:%i] popping from op. stack: %i\n", stack->top, stack->buffer[stack->top]);
		#endif
		switch (stack->buffer[stack->top]) {
			case OP_ADD:          write_instruction(writer, INST_ADD);          break;
			case OP_SUBTRACT:     write_instruction(writer, INST_SUBTRACT);     break;
			case OP_MULTIPLY:     write_instruction(writer, INST_MULTIPLY);     break;
			case OP_MODULO:       write_instruction(writer, INST_MODULO);       break;
			case OP_DIVIDE:       write_instruction(writer, INST_DIVIDE);       break;
			case OP_UNARY_MINUS:  write_instruction(writer, INST_UNARY_MINUS);  break;
			case OP_NOT:          write_instruction(writer, INST_NOT);          break;
			case OP_AND:          write_instruction(writer, INST_AND);          break;
			case OP_OR:           write_instruction(writer, INST_OR);           break;
			case OP_XOR:          write_instruction(writer, INST_XOR);          break;
			case OP_EQUAL: 	      write_instruction(writer, INST_EQUAL);        break;
			case OP_GREATER_THAN: write_instruction(writer, INST_GREATER_THAN); break;
			case OP_GREATER_EQUAL:write_instruction(writer, INST_GREATER_EQUAL);break;
			case OP_LESS_THAN:    write_instruction(writer, INST_LESS_THAN);    break;
			case OP_LESS_EQUAL:   write_instruction(writer, INST_LESS_EQUAL);   break;

			case OP_NOT_EQUAL:    write_instruction(writer, INST_EQUAL); 
								  write_instruction(writer, INST_NOT);          break;
		}
		stack->top--;
	}
	#ifdef DEEP_DEBUG
		printf("[DEEP DEBUG] operator_stack_pop_until(): Popped %i operations\n", top_before - stack->top);
	#endif
}

void parse_expression(ProgramReader *reader, BytecodeWriter *writer, char *variable_names) {
	#ifdef DEEP_DEBUG
		printf("[DEEP DEBUG] Parsing expression from line %i char %i\n", reader->line_number, reader->character_number);
	#endif
	OperatorStack operator_stack;
	int open_bracket_count;
	int number;
	char variable_name[MAX_VARIABLE_LENGTH];
	int stack_push_result = 0;

	/* TODO: updatenut tento komentar aby zodpovedal realite

		vyraz citame po skupinkach, ktore vyzeraju ako -(x)*,
		kde x je premenna alebo cislo a * je binarna operacia.
		-() sa mozu na tych miestach lubovolne opakovat a na zaciatku aj striedat
		(mozno chceme zakazat -- lebo je to divne),
		ale naopak x a * su povinne a musia tam byt prave raz.
		Okrem toho namiesto operacie moze byt proste koniec vyrazu,
		ktory moze byt bud neznamy znak (tie legit by boli napr. \n, <, zaciatok "alebo", ...),
		alebo unmatched zatvarajuca zatvorka (lebo ona moze byt sucast logickeho vyrazu ktory
		v sebe obsahuje toto, napriklad `nie (a < b alebo b < c)`).

		Druha moznost je spravit z toho goto spagety, lebo je to pain vyjadrovat cyklami,
		co sa kedy ma diat. Ale je to stavovy automat, tak to neni az take hrozne snad

		TRIK: mozem si na zaciatok dat na stack otvarajucu zatvorku a potom ked zistim
		ze je koniec, tak urobit to iste jak keby som videl zatvarajucu zatvorku,
		nech mozem riesit menej caseov a tiez nemusel checkovat underflow */

	operator_stack.top = 0;
	operator_stack.buffer[0] = OP_BEGIN_GROUP;
	open_bracket_count = 0; /* pocet zatvoriek, co zatial zacali a este neskoncili */

	/* Tu sa vo vyraze nachadzame pred nejakou hodnotou, teda tam moze byt unarne minus
		a zaciatky zatvoriek. */
	before_value:
	if (read_char(reader, '-')) 
		stack_push_result = operator_stack_push(&operator_stack, OP_UNARY_MINUS);
	if (read_char(reader, '(')) {
		open_bracket_count++;
		stack_push_result = operator_stack_push(&operator_stack, OP_BEGIN_GROUP);
		if (stack_push_result == EOF)
			compile_error(reader, "Stack overflow, (probably too many operations)", true);
		goto before_value;
	}

	/* Teraz nasleduje samotna hodnota, co je bud cislo... */
	if (read_number(reader, &number)) {
		if (number < 256) {
			write_instruction(writer, INST_PUSH);
			write_instruction(writer, number);
		}
		else if (number < 65536) {
			write_instruction(writer, INST_PUSH_2);
			write_instruction(writer, number << 8);
			write_instruction(writer, number & 255);
		}
		else {
			int shift;
			write_instruction(writer, INST_PUSH_4);
			for (shift = 24; shift > 0; shift -= 8) {
				write_instruction(writer, (number >> shift) & 255);
			}
		}
	}
	/* keyword minule bude nasledovany tahom (napr. kamen) a jeho hodnota je 1 ak tento tah bol zahraty a 0 ak nebol */
	else if (read_keyword(reader, "minule")) {
		write_instruction(writer, INST_LOAD);
		write_instruction(writer, ADDRESS_OPPONENTS_LAST_PLAY);
		write_instruction(writer, INST_PUSH);
		if (read_keyword(reader, "kamen")) write_instruction(writer, 0);
		else if (read_keyword(reader, "papier")) write_instruction(writer, 1);
		else if (read_keyword(reader, "noznice")) write_instruction(writer, 2);
		else compile_error(reader, "Klucove slovo `minule` musi byt nasledovane menom tahu: `kamen`, `papier` alebo `noznice`", true);
		write_instruction(writer, INST_EQUAL);
	}
	/* alebo unarna negacia (musi byt pred nacitanim premennej aby sa keyword 'not' nepovazoval za premennu) */
	else if (read_char(reader, '!') || read_keyword(reader, "not") || read_keyword(reader, "nie")) {
		parse_expression(reader, writer, variable_names);
		// operator_stack_pop_until(&operator_stack, writer, OP_NOT);
		stack_push_result = operator_stack_push(&operator_stack, OP_NOT);
	}
	/* ...alebo premenna. */
	else if (read_word(reader, variable_name)) {
		int variable_id;
		variable_id = variable_id_by_name(reader, variable_names, variable_name);
		write_instruction(writer, INST_LOAD);
		write_instruction(writer, variable_id);
	}
	else compile_error(reader, "mala by nasledovat hodnota, teda cislo, premenna alebo vyraz v zatvorke", true);

	/* Tu sa nachadzame po hodnote, teda tu mozu byt konce zatvoriek
		alebo operacie. */
	after_value:
	if (read_char(reader, ')')) {
		// Tento if statement nefunguje ked mame vyraz so zatrvorkou, ktory potom pojkracuje: (1 + 2) - 3
		// ked skonci zatrvorka, parser exitne a kompilator ocakava novy riadok

		if (open_bracket_count == 0) {
			/* Ak vidime unmatched zatvarajucu zatvorku, tak ju berieme ze neni sucast
				tohoto vyrazu, a teda mozeme skoncit. To je preto ze ona moze byt sucast
				nejakeho vonkajsieho vyrazu, napriklad ked mam podmienku a v nej vyraz
				ako `nie (a < b alebo b < (c + d))`, tak ta posledna zatvorka patri
				k tomu `nie (...)`, a nie k `(c + d)`. Ak je ta zatvorka naozaj unmatched,
				tak si to poriesi ten kod ktory cita vstup po nej. */
			ProgramReader_ungetc(')', reader);
			goto end;
		}
		else {
			operator_stack_pop_until(&operator_stack, writer, 1);
			operator_stack.top--;
			open_bracket_count--;
			goto after_value;
		}
	}

	if (read_char(reader, '+')) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_ADD_SUBTRACT);
		stack_push_result = operator_stack_push(&operator_stack, OP_ADD);
	}
	else if (read_char(reader, '-')) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_ADD_SUBTRACT);
		stack_push_result = operator_stack_push(&operator_stack, OP_SUBTRACT);
	}
	else if (read_char(reader, '*')) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_MULTIPLY_DIVIDE);
		stack_push_result = operator_stack_push(&operator_stack, OP_MULTIPLY);
	}
	else if (read_char(reader, '/')) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_MULTIPLY_DIVIDE);
		stack_push_result = operator_stack_push(&operator_stack, OP_DIVIDE);
	}
	else if (read_char(reader, '%')) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_MULTIPLY_DIVIDE);
		stack_push_result = operator_stack_push(&operator_stack, OP_MODULO);
	}
	else if (read_char(reader, '=')) {
		if (read_char(reader, '=')) {
			operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_COMPARE);
			stack_push_result = operator_stack_push(&operator_stack, OP_EQUAL);
		} else {
			compile_error(reader, "Unknown operation", true);
		}

	} else if (read_char(reader, '<')) {
		if (read_char(reader, '=')) {
			operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_COMPARE);
			stack_push_result = operator_stack_push(&operator_stack, OP_LESS_EQUAL);
		} else {
			operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_COMPARE);
			stack_push_result = operator_stack_push(&operator_stack, OP_LESS_THAN);
		}

	} else if (read_char(reader, '>')) {
		if (read_char(reader, '=')) {
			operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_COMPARE);
			stack_push_result = operator_stack_push(&operator_stack, OP_GREATER_EQUAL);
		} else {
			operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_COMPARE);
			stack_push_result = operator_stack_push(&operator_stack, OP_GREATER_THAN);
		}
	}
	else if (read_string(reader, "||") || read_keyword(reader, "or") || read_keyword(reader, "alebo")) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_LOGICAL_OP);
		stack_push_result = operator_stack_push(&operator_stack, OP_OR);
	}
	else if (read_string(reader, "&&") || read_keyword(reader, "and") || read_keyword(reader, "aj")) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_LOGICAL_OP);
		stack_push_result = operator_stack_push(&operator_stack, OP_AND);
	}
	else if (read_string(reader, "!=")) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_COMPARE);
		stack_push_result = operator_stack_push(&operator_stack, OP_NOT_EQUAL);
	}
	else if (read_char(reader, '^') || read_keyword(reader, "xor")) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_LOGICAL_OP);
		stack_push_result = operator_stack_push(&operator_stack, OP_XOR);
	}
	else if (read_keyword(reader, "!") || read_keyword(reader, "not") || read_keyword(reader, "nie")) {
		operator_stack_pop_until(&operator_stack, writer, OP_NOT);
		stack_push_result = operator_stack_push(&operator_stack, OP_NOT);
	}
	else goto end;

	/* TODO: pridat sem nejaky else-if ze ak vidime otvarajucu zatvorku, tak vyhlasime
		nejaku special case chybu ze tu nema byt. Lebo ocakavam ze deti mozno budu
		pisat veci ako a(b + c), cim myslia a * (b + c). Ale to nechceme dovolit,
		lebo ak dovolime taketo implicitne nasobenie tak to robi bordel inde,
		lebo neni jasne kedy je co nasobenie a kedy je to nieco ine. */
	/* Ked sa nam nepodarilo nacitat ziadny operator, tak to berieme ako koniec vyrazu */

	if (stack_push_result == EOF){
		compile_error(reader, "Stack overflow, (probably too many operations) - (you won't find help for this on https://stackoverflow.com)", true);
	}

	/* Ak sa nam naopak podarilo nacitat operator, tak zase ma nasledovat hodnota. */
	goto before_value;

	end:
	if (open_bracket_count > 0) compile_error(reader, "nezatvorena zatvorka", true);
	operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_LOGICAL_OP);
	
	#ifdef DEEP_DEBUG
		printf("[DEEP DEBUG] Finished parsing at line %i char %i\n", reader->line_number, reader->character_number);
	#endif
}

/* vrati dlzku bytecodu, co je uzitocne asi len na debugovanie
	TODO: spravit aby to bolo zase void? */
int compile(FILE *stream, int *memory, unsigned char *init_bytecode, unsigned char *program_bytecode, int total_rounds) {
	/* TODO: vysvetlit jak su ulozene nazvy premennych */
	char variable_names[VARIABLE_NAME_BUFFER_SIZE];
	int variable_count;

	/* Pre kazdy if si pamatame, kde zacina, aby ked dojdeme na koniec,
		tak sme vedeli napisat na jeho zaciatok, kam chceme skocit,
		ak ho preskakujeme. */
	int if_stack[MAX_NESTED_IFS];
	int if_stack_length;

	ProgramReader reader;
	BytecodeWriter writer;

	ProgramReader_init(&reader, stream);
	// reader.current_line_chars_length = 0;
	writer.buffer = init_bytecode;
	writer.length = 0;

	if_stack_length = 0;

	variable_count = 0;
	memset(variable_names, 0, VARIABLE_NAME_BUFFER_SIZE);
	read_new_line(&reader);

	// --- Predefine variables ---
	sprintf(&variable_names[variable_count * MAX_VARIABLE_LENGTH], "True");
	memory[variable_count] = 1;
	variable_count++;

	sprintf(&variable_names[variable_count * MAX_VARIABLE_LENGTH], "true");
	memory[variable_count] = 1;
	variable_count++;

	sprintf(&variable_names[variable_count * MAX_VARIABLE_LENGTH], "False");
	memory[variable_count] = 0;
	variable_count++;

	sprintf(&variable_names[variable_count * MAX_VARIABLE_LENGTH], "false");
	memory[variable_count] = 0;
	variable_count++;

	sprintf(&variable_names[variable_count * MAX_VARIABLE_LENGTH], "TOTAL_ROUNDS");
	memory[variable_count] = total_rounds;
	variable_count++;

	/* sekcia na zaciatku: nastavovanie premennych */
	while (!read_keyword(&reader, "program:")) {
		// --- DEBUG ---
		if (read_keyword(&reader, "!debug")) {
			#ifndef DEBUG
				compile_error(&reader, "`!debug` prikaz sa moze pouzit iba v debug mode", true);
			#endif
			if (read_keyword(&reader, "assert")) {
				parse_expression(&reader, &writer, variable_names);
				write_instruction(&writer, INST_DEBUG_ASSERT);
				write_instruction(&writer, reader.line_number << 8);
				write_instruction(&writer, reader.line_number & 255);
			} else if (read_keyword(&reader, "print")) {
				parse_expression(&reader, &writer, variable_names);
				write_instruction(&writer, INST_DEBUG_PRINT);
			} else {
				compile_error(&reader, "za !debug prikazom sa ocakava `assert` alebo `print`", true);
			}
			read_new_line(&reader);
			continue;
		}
		// --- DEBUG END ---
		if (!read_keyword(&reader, "nech")) {
			compile_error(&reader, "v sekcii init musia vsetky riadky zacinat klucovym slovom 'nech'", true);
		}
		int i; /* loop counter */
		char name[MAX_VARIABLE_LENGTH]; /* nazov premennej */
		int value; /* initial value premennej */

		/* TODO: skontrolovat ci mame este miesto na premennu */
		/* nazov premennej */
		if (!read_word(&reader, name))
			compile_error(&reader, "nazov premennej musi zacinat pismenom", true);
		/* TODO: ak sme nenacitali slovo (teda name je prazdny string),
			tak chceme vyhlasit chybu */
		/* skontrolujeme, ci premenna nema rovnaky nazov ako nejaka predosla */
		for (i = 0; i < variable_count; i++) {
			if (strcmp(name, &variable_names[i * MAX_VARIABLE_LENGTH]) == 0) {
				compile_error(&reader, "Premenna uz bola definovana", true);
			}
		}
		/* zatial variable_count pouzivame ako index tej novej premennej,
			na konci ho zvacsime o 1 */
		sprintf(&variable_names[variable_count * MAX_VARIABLE_LENGTH], "%s", name);
		if (!read_char(&reader, '='))
			compile_error(&reader, "za premennou musi byt =", true);
		// if (!read_number(&reader, &value))
		// 	compile_error(&reader, "premenna musi byt nastavena na nejake cislo", true);
		memory[variable_count] = VARIABLE_UNINITIALIZED;
		parse_expression(&reader, &writer, variable_names);
		write_instruction(&writer, INST_STORE);
		write_instruction(&writer, variable_count);
		variable_count++;
		if (!read_new_line(&reader))
			compile_error(&reader, "za hodnotou premennej musi byt koniec riadku", true);
	}
	if (!read_new_line(&reader))
		compile_error(&reader, "za direktivou 'program:' musi nasledovat novy riadok", true);

	/* Now write to the program bytecode */
	writer.buffer = program_bytecode;
	writer.length = 0;

	while (!feof(reader.stream)) {
		/* TODO: mozno sa tu chceme pozriet, ci neni nejaky ferror,
			a ak je tak to zabalit? */
		//printf("statement: citame riadok %d\n", reader.line_number);
		
		// --- DEBUG ---
		if (read_keyword(&reader, "!debug")) {
			#ifndef DEBUG
				compile_error(&reader, "`!debug` prikaz sa moze pouzit iba v debug mode", true);
			#endif
			if (read_keyword(&reader, "assert")) {
				parse_expression(&reader, &writer, variable_names);
				write_instruction(&writer, INST_DEBUG_ASSERT);
				write_instruction(&writer, reader.line_number << 8);
				write_instruction(&writer, reader.line_number & 255);
			} else if (read_keyword(&reader, "print")) {
				parse_expression(&reader, &writer, variable_names);
				write_instruction(&writer, INST_DEBUG_PRINT);
			} else {
				compile_error(&reader, "za !debug prikazom sa ocakava `assert` alebo `print`", true);
			}
			read_new_line(&reader);
			continue;
		}
		// --- DEBUG END ---

		if (read_keyword(&reader, "zahraj")) {
			if (read_keyword(&reader, "kamen"))
				write_instruction(&writer, INST_PLAY_ROCK);
			else if (read_keyword(&reader, "papier"))
				write_instruction(&writer, INST_PLAY_PAPER);
			else if (read_keyword(&reader, "noznice"))
				write_instruction(&writer, INST_PLAY_SCISSORS);
			else compile_error(&reader, "treba zahrat kamen, papier alebo noznice", true);
		}
		else if (read_keyword(&reader, "ak")) {
			if (if_stack_length >= MAX_NESTED_IFS)
				compile_error(&reader, "prilis vela vnorenych podmienok", true);
			// parse_condition(&reader, &writer, variable_names);
			parse_expression(&reader, &writer, variable_names);
			write_instruction(&writer, INST_JUMP_IF_ZERO_2);
			/* dalsie dva byty su ze kam skocime ak if neplati,
				co sa dozvieme az potom co precitame vnutro ifu,
				takze to doplnime az potom */
			if_stack[if_stack_length] = writer.length;
			if_stack_length++;
			write_instruction(&writer, 0);
			write_instruction(&writer, 0);
		}
		else if (read_keyword(&reader, "koniec")) {
			int if_start;
			assert(if_stack_length > 0);
			if_stack_length--;
			if_start = if_stack[if_stack_length];
			writer.buffer[if_start] = writer.length >> 8;
			writer.buffer[if_start + 1] = writer.length & 255;
		}
		else {
			/* Ak sme neprecitali ziadne klucove slovo, tak to musi byt assignment */
			int lhs_id;
			char lhs_name[MAX_VARIABLE_LENGTH];
			read_word(&reader, lhs_name);
			lhs_id = variable_id_by_name(&reader, variable_names, lhs_name);
			if (!read_char(&reader, '='))
				compile_error(&reader, "po premennej musi byt =", true);
			parse_expression(&reader, &writer, variable_names);
			write_instruction(&writer, INST_STORE);
			write_instruction(&writer, lhs_id);
		}
		if (!read_new_line(&reader))
			compile_error(&reader, "na konci prikazu ma byt koniec riadku", true);
	}
	if (if_stack_length > 0) compile_error(&reader, "k nejakemu `ak` chyba `koniec`", false);
	return writer.length;
}

/* Vypise instrukcie v skompilovanom bytecode na standardny vystup
	v takom peknom formate aby to clovek vedel precitat */
void dump_bytecode(unsigned char *bytecode, int length) {
	int i, n;
	for (i = 0; i < length; i++) {
		printf("%3d: ", i);
		switch (bytecode[i]) {
			int j, n;
			case INST_PLAY_ROCK:      printf("zahraj kamen\n");    break;
			case INST_PLAY_PAPER:     printf("zahraj papier\n");   break;
			case INST_PLAY_SCISSORS:  printf("zahraj noznice\n");  break;
			case INST_PUSH:
				i++;
				printf("push %d\n", bytecode[i]);
			break;
			case INST_PUSH_2:
				i++;
				n = bytecode[i] << 8;
				i++;
				n += bytecode[i];
				printf("push(2) %d\n", n);
			break;
			case INST_PUSH_4:
				n = 0;
				for (j = 0; j < 4; j++) {
					i++;
					n = (n << 8) + bytecode[i];
				}
				printf("push(4) %d\n", n);
			break;
			case INST_LOAD:
				i++;
				printf("load %d\n", bytecode[i]);
			break;
			case INST_STORE:
				i++;
				printf("store %d\n", bytecode[i]);
			break;
			case INST_ADD:            printf("add\n");               break;
			case INST_SUBTRACT:       printf("subtract\n");          break;
			case INST_UNARY_MINUS:    printf("unary minus\n");       break;
			case INST_MULTIPLY:       printf("multiply\n");          break;
			case INST_DIVIDE:         printf("divide\n");            break;
			case INST_MODULO:         printf("modulo\n");            break;
			case INST_EQUAL:          printf("equal\n");             break;
			case INST_LESS_THAN:      printf("less than\n");         break;
			case INST_LESS_EQUAL:     printf("less or equal\n");     break;
			case INST_GREATER_THAN:   printf("greater than\n");      break;
			case INST_GREATER_EQUAL:  printf("greater or equal\n");  break;
			case INST_OR:             printf("or\n");                break;
			case INST_AND:            printf("and\n");               break;
			case INST_NOT:            printf("not\n");               break;
			case INST_XOR:            printf("xor\n");               break;
			case INST_JUMP_IF_ZERO_2:
				i++;
				n = bytecode[i] << 8;
				i++;
				n += bytecode[i];
				printf("jump if zero(2) %d\n", n);
				break;
			case INST_DEBUG_ASSERT:
				printf("debug assert\n");
				i += 2;
				break;
			case INST_DEBUG_PRINT:
				printf("debug print value of expression\n");
				break;
			default:
				printf("Unknown instruction (id: %i)\n", bytecode[i]);
		}
	}
}

int to_bool(int num) {
	// Why do it like this? Source: https://stackoverflow.com/questions/39730583/return-value-of-a-boolean-expression-in-c?utm_source=gemini
	return num != 0;
}

int run(int *memory, unsigned char *bytecode) {
	int stack[STACK_SIZE];
	int stack_length;
	int i; /* kde v programe sme */
	int j, n;
	stack_length = 0;
	i = 0;
	while (1) {
		/* DEBUGGING --------- */
		#ifdef DEEP_DEBUG
		printf("%3d: stack: ", i);
		for (j = 0; j < stack_length; j++) printf("%d ", stack[j]);
		printf("\n");
		#endif
		/* ------------------- */
		switch (bytecode[i++]) {
			case INST_NULL:           return TURN_ERROR; /* Error, nothing was returned and the program ended */
			case INST_PLAY_ROCK:      return TURN_ROCK;
			case INST_PLAY_PAPER:     return TURN_PAPER;
			case INST_PLAY_SCISSORS:  return TURN_SCISSORS;
			case INST_PUSH:
				stack[stack_length] = bytecode[i++];
				stack_length++;
			break;
			case INST_PUSH_2:
				n = bytecode[i++] << 8;
				n += bytecode[i++];
				stack[stack_length] = n;
				stack_length++;
			break;
			case INST_PUSH_4:
				n = 0;
				for (j = 0; j < 4; j++) {
					n = (n << 8) + bytecode[i++];
				}
				stack[stack_length] = n;
				stack_length++;
			break;
			case INST_LOAD:
				stack[stack_length] = memory[bytecode[i++]];
				stack_length++;
			break;
			case INST_STORE:
				stack_length--;
				memory[bytecode[i++]] = stack[stack_length];
			break;
			case INST_ADD:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] + stack[stack_length];
			break;
			case INST_SUBTRACT:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] - stack[stack_length];
			break;
			case INST_MULTIPLY:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] * stack[stack_length];
			break;
			case INST_DIVIDE:
				stack_length--;
				if (stack[stack_length] == 0) {
					printf("Division by zero: Halting.\n");
					return TURN_ERROR;
				}
				stack[stack_length - 1] = stack[stack_length - 1] / stack[stack_length];
			break;
			case INST_MODULO:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] % stack[stack_length];
			break;
			case INST_UNARY_MINUS:
				stack[stack_length - 1] = -stack[stack_length - 1];
			break;
			case INST_EQUAL:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] == stack[stack_length];
			break;
			case INST_LESS_THAN:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] < stack[stack_length];
			break;
			case INST_LESS_EQUAL:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] <= stack[stack_length];
			break;
			case INST_GREATER_THAN:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] > stack[stack_length];
			break;
			case INST_GREATER_EQUAL:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] >= stack[stack_length];
			break;
			case INST_OR:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] || stack[stack_length];
			break;
			case INST_AND:
				stack_length--;
				stack[stack_length - 1] = stack[stack_length - 1] && stack[stack_length];
			break;
			case INST_NOT:
				stack[stack_length - 1] = !stack[stack_length - 1];
			break;
			case INST_XOR:
				stack_length--;
				stack[stack_length - 1] = to_bool(stack[stack_length - 1]) ^ to_bool(stack[stack_length]);
			case INST_JUMP_IF_ZERO_2:
				n = bytecode[i++] << 8;
				n += bytecode[i++];
				stack_length--;
				if (stack[stack_length] == 0) i = n;
			break;
			case INST_DEBUG_ASSERT:
				if (stack[stack_length - 1] == 0) {
					int line;
					line = bytecode[i++] << 8;
					line += bytecode[i++];
					printf("Assertion on line %i (instruction %i) FAILED. Returning error.\n", line, i);
					return TURN_ASSERT_FAILED;
				}
				i += 2;
				stack_length--;
			break;
			case INST_DEBUG_PRINT:
				printf("[INST_DEBUG_PRINT] Value on top of the stack: `%i`\n", stack[stack_length - 1]);
				stack_length--;
			break;
		}
	}
}

/** 
	Determines the result of match.

	returns: 
		> 0 - draw
		> 1 - turn 1 won
		> 2 - turn 2 won
		> -1 - turn 1 errored out (player 1 lost the game, end of game)
		> -2 - turn 2 errored out (player 2 lost the game, end of game)
		> -3 - both turns errored out (draw, end of game)
		> -100 - invalid turns, (valid turns are defined in TURN_ROCK, TURN_PAPER, TURN_SCISSORS, TURN_ERROR) or undefined combination (shouldn't happen)
*/
int who_won_round(char turn1, char turn2) {
	if (turn1 != TURN_ROCK && turn1 != TURN_PAPER && turn1 != TURN_SCISSORS && turn1 != TURN_ERROR && turn1 != TURN_ASSERT_FAILED) return -100;
	if (turn2 != TURN_ROCK && turn2 != TURN_PAPER && turn2 != TURN_SCISSORS && turn2 != TURN_ERROR && turn2 != TURN_ASSERT_FAILED) return -100;

	if (turn1 == turn2) {
		if (turn1 == TURN_ERROR || turn1 == TURN_ASSERT_FAILED) {
			return -3;
		}
		return 0;
	}
	else if (turn1 == TURN_ERROR || turn1 == TURN_ASSERT_FAILED) {
		return -1;
	}
	else if (turn2 == TURN_ERROR || turn2 == TURN_ASSERT_FAILED) {
		return -2;
	}
	else if (turn1 == TURN_SCISSORS && turn2 == TURN_PAPER) return 1;
	else if (turn1 == TURN_SCISSORS && turn2 == TURN_ROCK) return 2;
	else if (turn1 == TURN_PAPER && turn2 == TURN_ROCK) return 1;
	else if (turn1 == TURN_PAPER && turn2 == TURN_SCISSORS) return 2;
	else if (turn1 == TURN_ROCK && turn2 == TURN_PAPER) return 2;
	else if (turn1 == TURN_ROCK && turn2 == TURN_SCISSORS) return 1;

	printf("Unexpected error: unknown turn combination: `%c` and `%c`\n", turn1, turn2);
	return -100;
}

char get_scores_from_game(int roundc, char* round_data, int scores[2]) {
	int score1 = 0;
	int score2 = 0;
	bool end = false;
	for (int round_i = 0; round_i < roundc && !end; round_i++) {
		int round_result = who_won_round(round_data[round_i * 2], round_data[round_i * 2 + 1]);
		switch (round_result) {
			case 0:
				break;
			case 1:
				score1++;
				break;
			case 2:
				score2++;
				break;
			case -1:
				score1 = -1;
				end = true;
				break;
			case -2:
				score2 = -1;
				end = true;
				break;
			case -3:
				score1 = -1;
				score2 = -1;
				end = true;
				break;
			case -100:
				printf("Error: Somehow invalid turn / turn combination: %c, %c", round_data[round_i * 2], round_data[round_i * 2 + 1]);
				return -1;
		}
	}
	scores[0] = score1;
	scores[1] = score2;
	return 0;
}

const char* get_turn_name(char turn) {
	const char* TURN_ROCK_STR = "KAMEN";
	const char* TURN_PAPER_STR = "PAPIER";
	const char* TURN_SCISSORS_STR = "NOZNICE";
	const char* TURN_ERROR_STR = "ERROR";

	switch (turn) {
		case (TURN_ROCK): return TURN_ROCK_STR;
		case (TURN_PAPER): return TURN_PAPER_STR;
		case (TURN_SCISSORS): return TURN_SCISSORS_STR;
		case (TURN_ERROR): return TURN_ERROR_STR;
		default: {
			return NULL;
		}
	}
}

bool doesFileExist(const char* filename) {
	return !access(filename, F_OK);
}

int makeFilenameUnique(char* filename) {
	if (!doesFileExist(filename)) return 0;
	#ifdef DEBUG
		printf("Creating unique filename from: `%s`\n", filename);
	#endif
	int file_extension_period_pos = -1;
	int i = 0;
	while (filename[i] != '\0') {
		if (filename[i] == '.') file_extension_period_pos = i;
		i++;
	}

	#ifdef DEBUG
		printf("makeFilenameUnique(): Period position in filename is %i\n", file_extension_period_pos);
	#endif

	char file_basename[MAX_OUTPUT_FILE_NAME_LENGHT];
	if (file_extension_period_pos != -1) {
		snprintf(file_basename, file_extension_period_pos + 1, "%s", filename);
		// file_basename[file_extension_period_pos + 1] = '\0';
	} else
		// snprintf(file_basename, strlen(filename)+1, "%s", filename);
	 	strcpy(file_basename, filename);
	file_basename[strlen(file_basename)] = '\0';
	#ifdef DEBUG
		printf("makeFilenameUnique(): File basename(%lu): `%s`\n", strlen(file_basename), file_basename);
	#endif
	
	char file_extension[MAX_OUTPUT_FILE_NAME_LENGHT]; // Yeet there the full length to avoid shananogans like this: `haha.immabreakyourcodefrfr ... [more nonsense] ... haha`, where the period is at the start of the filename
	// strncpy(file_extension, filename + file_extension_period_pos + 1, i - file_extension_period_pos);
	if (file_extension_period_pos != -1)
		snprintf(file_extension, strlen(filename) - file_extension_period_pos, "%s", filename + file_extension_period_pos + 1);
	else
	 	file_extension[0] = '\0';
	#ifdef DEBUG
		printf("makeFilenameUnique(): File extension: `%s`\n", file_extension);
	#endif
	
	char file_fullname[MAX_OUTPUT_FILE_NAME_LENGHT + 10]; // +10 - leave space for the index number
	
	for (int j = 1; j < 1000000000; j++) {
		if (file_extension_period_pos != -1)
			sprintf(file_fullname, "%s#%i.%s", file_basename, j, file_extension);
		else
		 	sprintf(file_fullname, "%s#%i", file_basename, j);
		#ifdef DEBUG
			printf("Trying file #%i: `%s`\n", j, file_fullname);
		#endif
		bool avaible = !doesFileExist(file_fullname);
		if (avaible) {
			strcpy(filename, file_fullname);
			return 0;
		}
	}
	return -1;
}

int save_gamedata(const char* name1, const char* name2, int roundc, char* turn_data, const char* output_file_template, char* output_file, int* scores) {
	if (strlen(output_file_template) > MAX_OUTPUT_FILE_NAME_LENGHT) {printf("output_file_raw too long, max %i", MAX_OUTPUT_FILE_NAME_LENGHT); return -1;}
	strcpy(output_file, output_file_template);
	makeFilenameUnique(output_file);

	get_scores_from_game(roundc, turn_data, scores);
	FILE *file;
	file = fopen(output_file, "w");
	if (!file){
		printf("Error: Failed to create file `%s`.\n", output_file);
		return -1;
	}
	fprintf(file, "%s\n", name1);
	fprintf(file, "%s\n", name2);
	fprintf(file, "%i %i", scores[0], scores[1]);
	for (int i = 0; i < roundc; i++){
		const char* turn1 = get_turn_name(turn_data[i * 2]);
		if (turn1 == NULL) {
			printf("Error: save_game(): unknown turn %i: `%c` (ASCII: %i)", i, turn_data[i * 2], (int)turn_data[i * 2]);
			fclose(file);
			return -2;
		}
		const char* turn2 = get_turn_name(turn_data[i * 2 + 1]);
		if (turn2 == NULL){
			printf("Error: save_game(): unknown turn %i: `%c` (ASCII: %i)", i, turn_data[i * 2 + 1], (int)turn_data[i * 2 + 1]);
			fclose(file);
			return -2;
		}
		fprintf(file, "\n%s %s", turn1, turn2);
	}
	fclose(file);
	return 0;
}

const char HELP_MSG[] = "\nUsage: ./interpreter [flags] file(s)\n"
						"Flags:\n"
						" --help / -h: Print this message and exit\n\n"
						" --output / -o: Output file name (.txt extension is reccomended), default: `game_replay/game.txt`\n"
						"                Note: Output file will never overwrite another, it will always be made unique by adding #[number] to it (e.g.: game.txt -> game#2.txt)\n"
						"                Warning: Try to not do weird things with paths (e.g.: ~/../home/Documents/../Pictures/g.txt), this wasn't tested properly (yet)\n\n"
						" -r <ROUNDS>: (default: 50) Number of rounds played (if one of players errors out or doesn't play a turn, the game will be ended early)\n"
						" --names / -n: Provide names of players separated by space (default: player1 and player2).\n"
						"               Order: In manual mode, the first name is of human and the second of program\n"
						"                      In program mode, names belong to programs in same order as provided argument files\n\n"
						" -m: Manual mode: You will combat program, you need to provide one program file as an argument\n"
						" -p: Program mode (default): Two programs combat each other, you need to provide two program files as arguments\n\n"
						"Files:\n"
						"  In manual mode,  provide one file\n"
						"  In program mode, provide two files\n\n"
						"Example usage:\n"
						"$ ./interpreter -m -r 10 -n human bot examples/example_program.txt\n"
						"$ ./interpreter -r 50 --names botA botB --output game_replay/epic_game.txt examples/example_program.txt examples/example_program2.txt";
int main(int argc, char **argv) {
	#ifdef DEBUG
		printf("Running in debug mode...\n\n");
	#endif
	const char *play_names[4] = {"kamen", "papier", "noznice", "ERROR"};
	const char *DEFAULT_OUTPUT_FILE = "game_replay/game.txt";
	const char *DEFAULT_PLAYER1_NAME = "player1";
	const char *DEFAULT_PLAYER2_NAME = "player2";

	int filename_arg_i = -1;
	int filename2_arg_i = -1;
	int player1_name_arg_i = -1;
	int player2_name_arg_i = -1;
	int output_file_arg_i = -1;
	int roundc = 50;
	char mode = '?';

	/* Parse arguments and flags */
	for (int arg_i = 1; arg_i < argc; arg_i++){
		if (argv[arg_i][0] == '-') {
			// Parse flags
			if (strcmp(argv[arg_i], "--help") == 0 || strcmp(argv[arg_i], "-h") == 0) {
				printf(HELP_MSG);
				return 0;
			}
			else if (strcmp(argv[arg_i], "-m") == 0) {mode = 'm';}
			else if (strcmp(argv[arg_i], "-p") == 0) {mode = 'p';}
			else if (strcmp(argv[arg_i], "-r") == 0) {
				if (arg_i + 1 == argc) {
					printf("Error: Flag -r is missing its value\n\n");
					printf(HELP_MSG);
					return 1;
				}
				roundc = atoi(argv[arg_i + 1]);
				if (roundc <= 0) {
					printf("Error: Flag -r got non-numeric value or value of 0, which is not allowed: `%s`\n\n", argv[arg_i+1]);
					printf(HELP_MSG);
					return 1;
				}
				arg_i++;
				continue;
			} else if (strcmp(argv[arg_i], "--names") == 0 || strcmp(argv[arg_i], "-n") == 0){
				if (arg_i + 2 >= argc) {
					printf("Error: Flag --names/-n requires two values, got one or none.\n");
					printf(HELP_MSG);
					return 1;
				}
				player1_name_arg_i = arg_i + 1;
				player2_name_arg_i = arg_i + 2;
				arg_i += 2;
				continue;
			} else if (strcmp(argv[arg_i], "--output") == 0 || strcmp(argv[arg_i], "-o") == 0) {
				if (arg_i + 1 == argc) {
					printf("Error: Flag --output / -o is missing its value\n\n");
					printf(HELP_MSG);
					return 1;
				}
				output_file_arg_i = arg_i + 1;
				arg_i++;
				continue;
			} else {
				printf("Error: Unknown flag `%s`\n\n", argv[arg_i]);
				printf(HELP_MSG);
				return 1;
			}
			
		} else {
			// Parse arguments
			if (filename_arg_i == -1) {

				filename_arg_i = arg_i;
			} else if (filename2_arg_i == -1) {
				filename2_arg_i = arg_i;
			} else {
				printf("Error: Too many arguments expected at most 2\n\n");
				printf(HELP_MSG);
				return 1;
			}
		}
	}
	/* Handle invalid arguments */
	#ifdef DEBUG
		if (mode == '?') printf("[DEBUG] No mode flag found, using default value of `p`\n");
		else printf("[DEBUG] mode flag found: `%c`\n", mode);
	#endif
	if (mode == '?') mode = 'p';

	#ifdef DEBUG
		printf("[DEBUG] Starting with configuration:\n mode='%c'\n rounds=%d\n", mode, roundc);
	#endif

	if (filename_arg_i == -1) {
		if (mode == 'p')
			printf("\nError: Two arguments expected for mode 'program', got none\n");
		else 
			printf("\nError: One argument expected for mode 'manual', got none\n\n");
		printf(HELP_MSG);
		return 1;
	}
	if (mode == 'p' && filename2_arg_i == -1) {
		printf("\n"
			"Error: Two arguments expected for mode 'program', got one\n"
			"(Hint: to enable manual mode (human vs program) add `-m` flag)\n\n");
		printf(HELP_MSG);
		return 1;
	}
	if (mode == 'm' && filename2_arg_i != -1) {
		printf("\n"
			"Error: One argument expected for mode 'manual', got two\n"
			"(Hint: add another program file or remove `-m` flag to disable manual mode)\n\n");
		printf(HELP_MSG);
		return 1;
	}
	const char* output_file = output_file_arg_i == -1 ? DEFAULT_OUTPUT_FILE : argv[output_file_arg_i];
	if (strlen(output_file) > MAX_OUTPUT_FILE_NAME_LENGHT) {
		printf("Output filename too long, maximum lenght: %i\n", MAX_OUTPUT_FILE_NAME_LENGHT);
		return 1;
	}
	const char* player1_name = player1_name_arg_i == -1 ? DEFAULT_PLAYER1_NAME : argv[player1_name_arg_i];
	const char* player2_name = player2_name_arg_i == -1 ? DEFAULT_PLAYER2_NAME : argv[player2_name_arg_i];





	char* turn_data = malloc(2 * roundc * sizeof(char));
	if (turn_data == NULL) {
		printf("Failed to allocate memory for player turns (%i bytes), exiting.", 2 * roundc);
		return 1;
	}

	if (mode == 'm') {
		int memory[MEMORY_SIZE];
		unsigned char init_bytecode[MAX_BYTECODE_LENGTH];
		unsigned char program_bytecode[MAX_BYTECODE_LENGTH];


		/* Nastaviť celé pole na inštrukciu NULL, aby sme náhodou nebežali hodnoty, ktoré tam boli pred alokáciou */
		memset(init_bytecode, INST_NULL, MAX_BYTECODE_LENGTH);
		memset(program_bytecode, INST_NULL, MAX_BYTECODE_LENGTH);

		int play;
		char play_char;
		int bot_play;

		if (!doesFileExist(argv[filename_arg_i])) {
			printf("Error: Provided file `%s` does not exist.", argv[filename_arg_i]);
			free(turn_data);
			return 1;
		}
		FILE *program_file = fopen(argv[filename_arg_i], "r");
		compile(program_file, memory, init_bytecode, program_bytecode, roundc);

		#ifdef DEBUG
			int i = 0;
			while (init_bytecode[i] != INST_NULL) { i++; };
			printf("[DEBUG] Successfully compiled program!\n\n----- BYTECODE DUMP -----\nInit bytecode:\n");
			dump_bytecode(init_bytecode, i);
			i = 0;
			while (program_bytecode[i] != INST_NULL) { i++; };
			printf("\nProgram bytecode:\n");
			dump_bytecode(program_bytecode, i);
			printf("----- BYTECODE DUMP END -----\n\n");
		#endif

		/* na zaciatku sa v predoslom kole nehralo nic, takze to chcem byt nieco ine jak 0,1,2 */
		memory[ADDRESS_OPPONENTS_LAST_PLAY] = -1;
		printf("Running init...\n");
		int init_turn = run(memory, init_bytecode);
		if (init_turn == TURN_ASSERT_FAILED) {
			printf("Assert failed in init. Exiting.\n");
			goto end_main;
		}
			
		printf("Done.\n\n");

		printf("Teraz mozes hrat proti tvojmu botovi. Napis na vstup znak `k`, `p` alebo `n` pre zahranie tahu, 'e' (Error) pre vzdanie sa alebo `!` pre ukončenie hry.\n");
		printf("(Hra sa %i kol)\n> ", roundc);


		int round_i;
		int score_human = 0;
		int score_program = 0;
		for (round_i = 0; round_i < roundc; round_i++) {
			if (!(scanf("%c", &play_char) > 0)) {break;}
			#ifdef DEBUG
				printf("Reading character '%i'\n", (int)play_char);
			#endif

			if (play_char == 'k') 
				play = TURN_ROCK;
			else if (play_char == 'p') 
				play = TURN_PAPER;
			else if (play_char == 'n') 
				play = TURN_SCISSORS;
			else if (play_char == 'e') {
				printf("Turn error\n");
				play = TURN_ERROR;
			}
			else if (play_char == '\n') {
				round_i--; // Nepocitajme to ako tah
				continue;
			}
			else if (play_char == '!') {
				printf("User ended game.\n");
				break;
			}
			else { 
				printf("Neznamy tah `%c`, platne tahy su: `k`, `p` alebo `n` pre zahranie tahu, 'e' (Error) pre vzdanie sa alebo `!` pre ukoncenie hry.\n", play_char); 
				round_i--; // Nepocitajme to ako tah
				continue; 
			}

			bot_play = run(memory, program_bytecode);
			if (bot_play == TURN_ERROR || bot_play == TURN_ASSERT_FAILED){
				bot_play = TURN_ERROR;
				// printf("Bot nezahral tah alebo chyboval, koniec hry.\n");
				// break;
			}
			int result = who_won_round(play, bot_play);
			#ifdef DEBUG
				printf("[DEBUG] result=%i\n", result);
			#endif
			printf("[Kolo %i] Ty: %s, Program: %s ", round_i, play_names[play], play_names[bot_play]);
			
			switch (result) {
				case 0:
					printf("[Remiza]"); 
					break;
				case 1:
					printf("[Vyhral si!]"); 
					score_human++; 
					break;
				case 2:
					printf("[Prehral si]"); 
					score_program++; 
					break;
				case -1:
					printf("\nZahral si Error, prehral si hru, koniec hry."); 
					score_human = -1; 
					break;
				case -2:
					printf("\nProgram bota spadol alebo nezahral tah, vyhral si hru, koniec hry."); 
					score_program = -1; 
					break;
				case -3:
					printf("\nTy si zahral Error a program bota spadol alebo nezahral tah, (nie je to cute ako sa matchujete v eroroch?), remiza, koniec hry!!"); 
					score_human = -1; 
					score_program = -1;
					break;
				case -100:
					printf("Achevement get: How did we get here? (this should never happen: Unknown move or move combination: %c, %c).", play, bot_play); break;
			}
			if (score_human == -1 || score_program == -1)
				printf(" Skore: Ty: %i, Program: %i\n", score_human, score_program);
			else
				printf(" Skore: Ty: %i, Program: %i\n> ", score_human, score_program);
			
			memory[ADDRESS_OPPONENTS_LAST_PLAY] = play;
			turn_data[round_i * 2] = play;
			turn_data[round_i * 2 + 1] = bot_play;

			if (play == TURN_ERROR || bot_play == TURN_ERROR) 
				break;
		}
		if (round_i == roundc) printf("\nVsetky kola boli zahrate (%d), koniec\n", round_i);
		else printf("\nHra bola ukoncena predcasne v kole %d\n\n", round_i);
		printf("Finalne skore: Ty: %i, Program: %i\nResult: ", score_human, score_program);
		if (score_human > score_program) {
			printf("Vyhral si!\n\n");
		} else if (score_human < score_program) {
			printf("Prehral si.\n\n");
		} else {
			printf("Remiza.\n\n");
		}

		printf("Prebieha ukladanie hry do suboru...\n");

		char output_file_unique[MAX_OUTPUT_FILE_NAME_LENGHT + 10]; // Tu sa zapise jedinecny nazov suboru
		/* Poznamka: Pouzivame round_i namiesto roundc, lebo ked ukoncime hru predcasne mame len round_i odohranych kol */
		int scores[2];
		int result = save_gamedata(player1_name, player2_name, round_i, turn_data, output_file, output_file_unique, scores);

		if (result == 0) // Success
			printf("Hra bola ulozena do: `%s`", output_file_unique);
		
		#ifdef DEBUG
			printf("\n GAME LOG:\n");
			for (int i = 0; i < round_i; i++) {
				printf("TURN %i: human: %s, bot: %s\n", i, play_names[turn_data[i*2]], play_names[turn_data[i*2+1]]);
			}
		#endif
		fclose(program_file);

	} else if (mode == 'p') {
		if (!doesFileExist(argv[filename_arg_i])) {
			printf("Error: Provided file `%s` does not exist.", argv[filename_arg_i]);
			free(turn_data);
			return 1;
		}
		if (!doesFileExist(argv[filename2_arg_i])) {
			printf("Error: Provided file `%s` does not exist.", argv[filename2_arg_i]);
			free(turn_data);
			return 1;
		}

		int program1_memory[MEMORY_SIZE];
		unsigned char program1_init_bytecode[MAX_BYTECODE_LENGTH], program1_program_bytecode[MAX_BYTECODE_LENGTH];
		memset(program1_init_bytecode, INST_NULL, MAX_BYTECODE_LENGTH);
		memset(program1_program_bytecode, INST_NULL, MAX_BYTECODE_LENGTH);

		int program2_memory[MEMORY_SIZE];
		unsigned char program2_init_bytecode[MAX_BYTECODE_LENGTH], program2_program_bytecode[MAX_BYTECODE_LENGTH];
		memset(program2_init_bytecode, INST_NULL, MAX_BYTECODE_LENGTH);
		memset(program2_program_bytecode, INST_NULL, MAX_BYTECODE_LENGTH);



		int program1_turn;
		int program2_turn;
		int result;

		#ifdef DEBUG
			printf("(program1) Compiling program `%s`...\n", argv[filename_arg_i]);
		#endif
		FILE* program1_file = fopen(argv[filename_arg_i], "r");
		compile(program1_file, program1_memory, program1_init_bytecode, program1_program_bytecode, roundc);
		#ifdef DEBUG
			printf("Compiled successfully.\n");

			printf("(program2) Compiling program `%s`...\n", argv[filename2_arg_i]);
		#endif
		FILE* program2_file = fopen(argv[filename2_arg_i], "r");
		compile(program2_file, program2_memory, program2_init_bytecode, program2_program_bytecode, roundc);
		#ifdef DEBUG
			printf("Compiled sucessfully.\n");
		#endif

		program1_memory[ADDRESS_OPPONENTS_LAST_PLAY] = 0;
		program2_memory[ADDRESS_OPPONENTS_LAST_PLAY] = 0;

		int init_turn = run(program1_memory, program1_init_bytecode);
		if (init_turn == TURN_ASSERT_FAILED) {
			printf("Assert failed in init of program %s. Exiting...\n", argv[filename_arg_i]);
			goto end_main;
		}
		init_turn = run(program2_memory, program2_init_bytecode);
		if (init_turn == TURN_ASSERT_FAILED) {
			printf("Assert failed in init of program %s. Exiting...\n", argv[filename_arg_i]);
			goto end_main;
		}

		for (int round_i = 0; round_i < roundc; round_i++) {
			program1_turn = run(program1_memory, program1_program_bytecode);
			program2_turn = run(program2_memory, program2_program_bytecode);
			turn_data[round_i * 2] = program1_turn;
			turn_data[round_i * 2 + 1] = program2_turn;

			result = who_won_round(program1_turn, program2_turn);
			#ifdef DEBUG
				printf("[DEBUG round %i] Turn1: %i, Turn2: %i, result: %i\n", round_i, program1_turn, program2_turn, result);
			#endif
			if (result < 0) {
				break; // Someone error out - end the game
			}
		}

		char output_file_unique[MAX_OUTPUT_FILE_NAME_LENGHT + 10]; // Tu sa zapise jedinecny nazov suboru
		int scores[2];
		save_gamedata(player1_name, player2_name, roundc, turn_data, output_file, output_file_unique, scores);
		
		printf("%i %i", scores[0], scores[1]);

		fclose(program1_file);
		fclose(program2_file);
	} else {
		printf("Unknown mode: `%c`", mode);
		free(turn_data);
		return 1;
	}
	end_main:
	free(turn_data);
	return 0;
}