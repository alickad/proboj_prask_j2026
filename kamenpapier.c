#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BYTECODE_LENGTH 16384
/* premenne v programe + "systemova" premenna, to co hrac zahral */
#define MEMORY_SIZE 64
/* maximalny pocet premennych, ktore si mozes v programe definovat */
#define MAX_VARIABLE_COUNT 63
/* maximalna dlzka nazvu premennej, vratane null terminatora */
#define MAX_VARIABLE_LENGTH 64
#define STACK_SIZE 64
#define MAX_NESTED_IFS 16

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
#define INST_EQUAL           13
#define INST_LESS_THAN       14
#define INST_LESS_EQUAL      15
#define INST_GREATER_THAN    16
#define INST_GREATER_EQUAL   17
#define INST_OR              18
#define INST_AND             19
#define INST_NOT             20
/* JUMP_IF_ZERO: zoberieme cislo zo stacku a ak je to 0, tak skocime na take miesto
	v programe, ako hovori parameter. Je iba verzia s 2 parametrami, lebo ked piseme
	tu instrukciu, tak nevieme dopredu, ako daleko bude ten if koncit. */
#define INST_JUMP_IF_ZERO_2  21

/* NULL: defaultná hodnota bytecode bufferu, neicializovaná hodnota, keď ju prečítame vieme ze program skončil */
#define INST_NULL            127

/* Kody aritmetickych operacii, co davame na stack.
	Musia byt zoradene podla prednosti (napriklad `+` ma prednost pred `*`),
	aby sa potom dali porovnavat.
	TODO: popisat jak presne funguje prednost, pri veciach jak zatvorky a unarne minus
	Pointa je ze veci mozu mat inu prednost ze koho mozu oni vyhodit zo stacku
	a ze kto moze vyhodit zo stacku ich.
	Begin group je zaciatok zatvorky alebo zaciatok celeho vyrazu*/
#define OP_BEGIN_GROUP  0
#define OP_ADD          1
#define OP_SUBTRACT     2
#define OP_UNARY_MINUS  3
#define OP_MULTIPLY     4
#define OP_DIVIDE       5

/* Logicke operacie, tie su z hladiska precedence nezavisle od aritmetickych,
	ale na popovanie zo stacku pouzivaju ten isty kod takze musia mat rozne cisla. */
#define OP_OR   6
#define OP_AND  7
#define OP_NOT  8

#define PRECEDENCE_ADD_SUBTRACT 1
#define PRECEDENCE_MULTIPLY_DIVIDE 4

#define TURN_ROCK 0
#define TURN_PAPER 1
#define TURN_SCISSORS 2
#define TURN_ERROR 127

typedef struct {
	FILE *stream;
	int line_number;
} ProgramReader;

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
		c = fgetc(reader->stream);
		if (c == EOF) return;
	} while (IS_WHITESPACE(c));
	ungetc(c, reader->stream);
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
	c = fgetc(reader->stream);
	if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
		success = 0;
		goto end;
	}
	for (i = 0; i < MAX_VARIABLE_LENGTH; i++) {
		*dest = c;
		dest++;
		c = fgetc(reader->stream);
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) goto end;
	}
	/* Ak sme sa dostali sem, tak mame na vstupe slovo, co je moc dlhe */
	/* TODO: compile error: moc dlhe slovo */
	/* Precitali sme znak, ktory uz nie je sucast slova, alebo sme na konci vstupu */
	end:
	if (c != EOF) ungetc(c, reader->stream);
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
	c = fgetc(reader->stream);
	if (c == EOF) return 0;
	if (c == target) {
		read_whitespace(reader);
		return 1;
	}
	ungetc(c, reader->stream);
	return 0;
}

int read_keyword(ProgramReader *reader, char *keyword) {
	int i;
	char c;
	for (i = 0; keyword[i] != '\0'; i++) {
		c = fgetc(reader->stream);
		if (c != keyword[i]) {
			if (c != EOF) ungetc(c, reader->stream);
			for (i--; i >= 0; i--) ungetc(keyword[i], reader->stream);
			return 0;
		}
	}
	/* Ak sa cele slovo zhoduje s keyword, este skontrolujeme,
		ci nahodou to slovo nepokracuje dalej, kedy by sa to neratalo */
	c = fgetc(reader->stream);
	if (c != EOF) ungetc(c, reader->stream);
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
		for (i--; i >= 0; i--) ungetc(keyword[i], reader->stream);
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
		c = fgetc(reader->stream);
		/* ak vidime komentar, tak citame az do konca riadku */
		if (c == '#') {
			do {
				c = fgetc(reader->stream);
			} while (c != '\n' && c != EOF);
			success = 1;
		}
		/* ak sme naposledy precitali novy riadok, tak si pamatame ze ho mame */
		if (c == '\n') {
			success = 1;
			reader->line_number++;
			read_whitespace(reader);
		}
		/* ak tam je iny znak alebo koniec vstupu, tak skoncime */
		else {
			if (c != EOF) ungetc(c, reader->stream);
			return success || c == EOF;
		}
	}
}

/* TODO: pridat tam aj nazov programu */
void compile_error(ProgramReader *reader, char *message) {
	printf("chyba pri citani programu, riadok %d: %s\n", reader->line_number, message);
	exit(EXIT_FAILURE);
}

/* Ak je na vstupe cislo (kladne cele), tak ho precita a ulozi do `dest` a vrati 1.
	Inak vstup necha tak a vrati 0.

	Ako kazda funkcia na citanie vstupu, ak je uspesna, tak precita aj nasledujuci whitespace.

	TODO: overflow check a tiez check ze nenasleduju rovno za cislicami pismena */
int read_number(ProgramReader *reader, int *dest) {
	int success;
	char c;
	success = 1;
	c = fgetc(reader->stream);
	if (c < '0' || c > '9') {
		success = 0;
		*dest = -1;
		goto end;
	}
	*dest = 0;
	do {
		*dest *= 10;
		*dest += c - '0';
		c = fgetc(reader->stream);
	} while (c >= '0' && c <= '9');
	end:
	if (c != EOF) ungetc(c, reader->stream);
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
	compile_error(reader, "neznamy nazov premennej");
}

/* TODO: pocitat si kolko by sme na to potrebovali stack spaceu pocas runtimeu,
	aby sme vedeli garantovat ze ked sa to skompiluje tak to bude ok. */
void operator_stack_push(OperatorStack *stack, int item) {
	stack->top++;
	/* TODO: checknut overflow */
	stack->buffer[stack->top] = item;
}

void operator_stack_pop_until(OperatorStack *stack, BytecodeWriter *writer, int precedence) {
	while (stack->buffer[stack->top] >= precedence) {
		switch (stack->buffer[stack->top]) {
			case OP_ADD:          write_instruction(writer, INST_ADD);          break;
			case OP_SUBTRACT:     write_instruction(writer, INST_SUBTRACT);     break;
			case OP_MULTIPLY:     write_instruction(writer, INST_MULTIPLY);     break;
			case OP_DIVIDE:       write_instruction(writer, INST_DIVIDE);       break;
			case OP_UNARY_MINUS:  write_instruction(writer, INST_UNARY_MINUS);  break;
			case OP_NOT:          write_instruction(writer, INST_NOT);          break;
			case OP_AND:          write_instruction(writer, INST_AND);          break;
			case OP_OR:           write_instruction(writer, INST_OR);           break;
		}
		stack->top--;
	}
}

void parse_expression(ProgramReader *reader, BytecodeWriter *writer, char *variable_names) {
	OperatorStack operator_stack;
	int open_bracket_count;
	int number;
	char variable_name[MAX_VARIABLE_LENGTH];

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
	if (read_char(reader, '-')) operator_stack_push(&operator_stack, OP_UNARY_MINUS);
	if (read_char(reader, '(')) {
		open_bracket_count++;
		operator_stack_push(&operator_stack, OP_BEGIN_GROUP);
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
	/* ...alebo premenna. */
	else if (read_word(reader, variable_name)) {
		int variable_id;
		variable_id = variable_id_by_name(reader, variable_names, variable_name);
		write_instruction(writer, INST_LOAD);
		write_instruction(writer, variable_id);
	}
	else compile_error(reader, "mala by nasledovat hodnota, teda cislo, premenna alebo vyraz v zatvorke");

	/* Tu sa nachadzame po hodnote, teda tu mozu byt konce zatvoriek
		alebo operacie. */
	after_value:
	if (read_char(reader, ')')) {
		if (open_bracket_count == 0) {
			/* Ak vidime unmatched zatvarajucu zatvorku, tak ju berieme ze neni sucast
				tohoto vyrazu, a teda mozeme skoncit. To je preto ze ona moze byt sucast
				nejakeho vonkajsieho vyrazu, napriklad ked mam podmienku a v nej vyraz
				ako `nie (a < b alebo b < (c + d))`, tak ta posledna zatvorka patri
				k tomu `nie (...)`, a nie k `(c + d)`. Ak je ta zatvorka naozaj unmatched,
				tak si to poriesi ten kod ktory cita vstup po nej. */
			ungetc(')', reader->stream);
			goto end;
		}
		else {
			operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_ADD_SUBTRACT);
			operator_stack.top--;
			open_bracket_count--;
			goto after_value;
		}
	}

	if (read_char(reader, '+')) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_ADD_SUBTRACT);
		operator_stack_push(&operator_stack, OP_ADD);
	}
	else if (read_char(reader, '-')) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_ADD_SUBTRACT);
		operator_stack_push(&operator_stack, OP_SUBTRACT);
	}
	else if (read_char(reader, '*')) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_MULTIPLY_DIVIDE);
		operator_stack_push(&operator_stack, OP_MULTIPLY);
	}
	else if (read_char(reader, '/')) {
		operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_MULTIPLY_DIVIDE);
		operator_stack_push(&operator_stack, OP_DIVIDE);
	}
	/* TODO: pridat sem nejaky else-if ze ak vidime otvarajucu zatvorku, tak vyhlasime
		nejaku special case chybu ze tu nema byt. Lebo ocakavam ze deti mozno budu
		pisat veci ako a(b + c), cim myslia a * (b + c). Ale to nechceme dovolit,
		lebo ak dovolime taketo implicitne nasobenie tak to robi bordel inde,
		lebo neni jasne kedy je co nasobenie a kedy je to nieco ine. */
	/* Ked sa nam nepodarilo nacitat ziadny operator, tak to berieme ako koniec vyrazu */
	else goto end;

	/* Ak sa nam naopak podarilo nacitat operator, tak zase ma nasledovat hodnota. */
	goto before_value;

	end:
	if (open_bracket_count > 0) compile_error(reader, "nezatvorena zatvorka");
	operator_stack_pop_until(&operator_stack, writer, PRECEDENCE_ADD_SUBTRACT);
}

void parse_condition(ProgramReader *reader, BytecodeWriter *writer, char *variable_names) {
	OperatorStack operator_stack;
	int open_bracket_count;

	operator_stack.top = 0;
	operator_stack.buffer[0] = OP_BEGIN_GROUP;
	open_bracket_count = 0; /* pocet zatvoriek, co zatial zacali a este neskoncili */

	/* Tu sa nachadzame pred hodnotou, takze tam moze byt `nie` a zaciatok zatvorky. */
	before_value:
	if (read_keyword(reader, "nie")) operator_stack_push(&operator_stack, OP_NOT);
	if (read_char(reader, '(')) {
		open_bracket_count++;
		operator_stack_push(&operator_stack, OP_BEGIN_GROUP);
		goto before_value;
	}

	/* Teraz ide samotna hodnota, co je bud `minule CO_ZAHRAL_HRAC` alebo VYRAZ POROVNANIE VYRAZ`. */
	if (read_keyword(reader, "minule")) {
		write_instruction(writer, INST_LOAD);
		write_instruction(writer, ADDRESS_OPPONENTS_LAST_PLAY);
		write_instruction(writer, INST_PUSH);
		if (read_keyword(reader, "kamen")) write_instruction(writer, 0);
		else if (read_keyword(reader, "papier")) write_instruction(writer, 1);
		else if (read_keyword(reader, "noznice")) write_instruction(writer, 2);
		else compile_error(reader, "treba zahrat kamen, papier alebo noznice");
		write_instruction(writer, INST_EQUAL);
	}
	else {
		/* Inak tu musi byt porovnanie */
		int comparison;
		parse_expression(reader, writer, variable_names);
		if (read_char(reader, '<')) {
			if (read_char(reader, '=')) comparison = INST_LESS_EQUAL;
			else comparison = INST_LESS_THAN;
		}
		else if (read_char(reader, '>')) {
			if (read_char(reader, '=')) comparison = INST_GREATER_EQUAL;
			else comparison = INST_GREATER_THAN;
		}
		else if (read_char(reader, '=')) {
			read_char(reader, '='); // Just in case ze niekto pouziva `==` namiesto `=`
			comparison = INST_EQUAL;
		}
		else compile_error(reader, "tu by malo byt porovnanie, teda < <= > alebo >=");
		parse_expression(reader, writer, variable_names);
		write_instruction(writer, comparison);
	}

	/* Tu moze byt koniec zatvorky alebo operacia alebo koniec */
	after_value:
	if (read_char(reader, ')')) {
		if (open_bracket_count == 0) compile_error(reader, "navyse zatvarajuca zatvorka");
		operator_stack_pop_until(&operator_stack, writer, OP_OR);
		operator_stack.top--;
		open_bracket_count--;
		goto after_value;
	}

	if (read_keyword(reader, "aj")) {
		operator_stack_pop_until(&operator_stack, writer, OP_AND);
		operator_stack_push(&operator_stack, OP_AND);
	}
	else if (read_keyword(reader, "alebo")) {
		operator_stack_pop_until(&operator_stack, writer, OP_OR);
		operator_stack_push(&operator_stack, OP_OR);
	}
	/* TODO: ak napises `a` miesto `aj` (co sa mi celkom stava),
		tak momentalne to iba assumuje ze je koniec, ale chcelo by to nejaky lepsi error reporting.
		Ono v tomto pripade koniec je aj tak len ked je koniec riadku,
		tak tu by to poradie checkovania mohlo byt reverznute */
	/* ak sa nam nepodarilo nacitat operator tak uz je koniec */
	else goto end;
	/* ak sa nam naopak podarilo nacitat operator tak sme zase pred operandom */
	goto before_value;

	end:
	if (open_bracket_count > 0) compile_error(reader, "nezatvorena zatvorka");
	operator_stack_pop_until(&operator_stack, writer, OP_OR);
}

/* vrati dlzku bytecodu, co je uzitocne asi len na debugovanie
	TODO: spravit aby to bolo zase void? */
int compile(FILE *stream, int *memory, unsigned char *bytecode) {
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

	reader.stream = stream;
	reader.line_number = 1;
	writer.buffer = bytecode;
	writer.length = 0;

	if_stack_length = 0;

	variable_count = 0;
	memset(variable_names, 0, VARIABLE_NAME_BUFFER_SIZE);
	read_new_line(&reader);

	/* sekcia na zaciatku: nastavovanie premennych */
	while (!read_keyword(&reader, "program:")) {
		if (!read_keyword(&reader, "nech")) {
			compile_error(&reader, "v sekcii init musia vsetky riadky zacinat klucovym slovom 'nech'");
		}
		int i; /* loop counter */
		char name[MAX_VARIABLE_LENGTH]; /* nazov premennej */
		int value; /* initial value premennej */

		/* TODO: skontrolovat ci mame este miesto na premennu */
		/* nazov premennej */
		if (!read_word(&reader, name))
			compile_error(&reader, "nazov premennej musi zacinat pismenom");
		/* TODO: ak sme nenacitali slovo (teda name je prazdny string),
			tak chceme vyhlasit chybu */
		/* skontrolujeme, ci premenna nema rovnaky nazov ako nejaka predosla */
		for (i = 0; i < variable_count; i++) {
			if (strcmp(name, &variable_names[variable_count * MAX_VARIABLE_LENGTH]) == 0) {
				/* TODO: compile error: rovnaky nazov */
			}
		}
		/* zatial variable_count pouzivame ako index tej novej premennej,
			na konci ho zvacsime o 1 */
		strcpy(&variable_names[variable_count * MAX_VARIABLE_LENGTH], name);
		if (!read_char(&reader, '='))
			compile_error(&reader, "za premennou musi byt =");
		if (!read_number(&reader, &value))
			compile_error(&reader, "premenna musi byt nastavena na nejake cislo");
		memory[variable_count] = value;
		variable_count++;
		if (!read_new_line(&reader))
			compile_error(&reader, "za hodnotou premennej musi byt koniec riadku");
	}
	if (!read_new_line(&reader))
		compile_error(&reader, "za direktivou 'program:' musi nasledovat novy riadok");

	while (!feof(reader.stream)) {
		/* TODO: mozno sa tu chceme pozriet, ci neni nejaky ferror,
			a ak je tak to zabalit? */
		//printf("statement: citame riadok %d\n", reader.line_number);
		if (read_keyword(&reader, "zahraj")) {
			if (read_keyword(&reader, "kamen"))
				write_instruction(&writer, INST_PLAY_ROCK);
			else if (read_keyword(&reader, "papier"))
				write_instruction(&writer, INST_PLAY_PAPER);
			else if (read_keyword(&reader, "noznice"))
				write_instruction(&writer, INST_PLAY_SCISSORS);
			else compile_error(&reader, "treba zahrat kamen, papier alebo noznice");
		}
		else if (read_keyword(&reader, "ak")) {
			if (if_stack_length == MAX_NESTED_IFS)
				compile_error(&reader, "prilis vela vnorenych podmienok");
			parse_condition(&reader, &writer, variable_names);
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
				compile_error(&reader, "po premennej musi byt =");
			parse_expression(&reader, &writer, variable_names);
			write_instruction(&writer, INST_STORE);
			write_instruction(&writer, lhs_id);
		}
		if (!read_new_line(&reader))
			compile_error(&reader, "na konci prikazu ma byt koniec riadku");
	}
	if (if_stack_length > 0) compile_error(&reader, "k nejakemu `ak` chyba `koniec`");
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
			case INST_MULTIPLY:       printf("multiply\n");          break;
			case INST_DIVIDE:         printf("divide\n");            break;
			case INST_EQUAL:          printf("equal\n");             break;
			case INST_LESS_THAN:      printf("less than\n");         break;
			case INST_LESS_EQUAL:     printf("less or equal\n");     break;
			case INST_GREATER_THAN:   printf("greater than\n");      break;
			case INST_GREATER_EQUAL:  printf("greater or equal\n");  break;
			case INST_OR:             printf("or\n");                break;
			case INST_AND:            printf("and\n");               break;
			case INST_NOT:            printf("not\n");               break;
			case INST_JUMP_IF_ZERO_2:
				i++;
				n = bytecode[i] << 8;
				i++;
				n += bytecode[i];
				printf("jump if zero(2) %d\n", n);
			break;
		}
	}
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
		#if 0
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
				stack[stack_length - 1] = stack[stack_length - 1] / stack[stack_length];
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
			case INST_JUMP_IF_ZERO_2:
				n = bytecode[i++] << 8;
				n += bytecode[i++];
				stack_length--;
				if (stack[stack_length] == 0) i = n;
			break;
		}
	}
}

char get_scores_from_game(int roundc, char* round_data, int scores[2]) {
	int score1 = 0;
	int score2 = 0;
	for (int round_i = 0; round_i < roundc; round_i++) {
		if (round_data[round_i * 2] == round_data[round_i * 2 + 1]) {
			if (round_data[round_i * 2] == TURN_ERROR) {
				score1 = -1;
				score2 = -1;
				break;
			}
			continue;
		}
		else if (round_data[round_i * 2] == TURN_ERROR) {
			score1 = -1;
			break;
		}
		else if (round_data[round_i * 2 + 1] == TURN_ERROR) {
			score2 = -1;
			break;
		}
		else if (round_data[round_i * 2] == TURN_SCISSORS && round_data[round_i * 2 + 1] == TURN_PAPER) score1++;
		else if (round_data[round_i * 2] == TURN_SCISSORS && round_data[round_i * 2 + 1] == TURN_ROCK) score2++;
		else if (round_data[round_i * 2] == TURN_PAPER && round_data[round_i * 2 + 1] == TURN_ROCK) score1++;
		else if (round_data[round_i * 2] == TURN_PAPER && round_data[round_i * 2 + 1] == TURN_SCISSORS) score2++;
		else if (round_data[round_i * 2] == TURN_ROCK && round_data[round_i * 2 + 1] == TURN_PAPER) score2++;
		else if (round_data[round_i * 2] == TURN_ROCK && round_data[round_i * 2 + 1] == TURN_SCISSORS) score1++;
		else {
			printf("Unexpected error: uknown turn combination: `%c` and `%c`\n", round_data[round_i * 2], round_data[round_i * 2 + 1]);
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
int save_gamedata(const char* name1, const char* name2, int roundc, char* turn_data, const char* output_file) {
	int scores[2];
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
			return -2;
		}
		const char* turn2 = get_turn_name(turn_data[i * 2 + 1]);
		if (turn2 == NULL){
			printf("Error: save_game(): unknown turn %i: `%c` (ASCII: %i)", i, turn_data[i * 2], (int)turn_data[i * 2]);
			return -2;
		}
		fprintf(file, "\n%s %s", turn1, turn2);
	}
	return 0;
}

const char HELP_MSG[] = "\nUsage: ./kamenpapier [flags] file(s)\n"
						"Flags:\n"
						" --help / -h: Print this message and exit\n\n"
						" --output / -o: Output file name (.txt extension is reccomended), default: `kamenpapier_game_replay.txt`\n\n"
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
						"$ ./kamenpapier -m -r 10 -n human bot -o example_output.txt example.psc\n"
						"$ ./kamenpapier -r 50 --names botA botB --output example_output.txt example.psc example2.psc";
int main(int argc, char **argv) {
	#ifdef DEBUG
		printf("Running in debug mode...\n\n");
	#endif
	FILE *file;
	int memory[MEMORY_SIZE];
	unsigned char bytecode[MAX_BYTECODE_LENGTH];


	/* Nastaviť celé pole na inštrukciu NULL, aby sme náhodou nebežali hodnoty, ktoré tam boli pred alokáciou */
	for (unsigned i = 0; i < MAX_BYTECODE_LENGTH; i++) {bytecode[i] = INST_NULL;}

	int play;
	char play_char;
	int bot_play;

	const char *play_names[3] = {"kamen", "papier", "noznice"};
	const char *DEFAULT_OUTPUT_FILE = "kamenpapier_game_replay.txt";
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
			// PARSE FLAGS
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
			// PARSE ARGUMENTS
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
		printf("\nError: At least one argument required, got none\n\n");
		printf(HELP_MSG);
		return 1;
	}
	if (mode == 'p' && filename2_arg_i == -1) {
		printf("\nError: Two arguments expected for mode 'program', got one\n\n");
		printf(HELP_MSG);
		return 1;
	}
	if (mode == 'm' && filename2_arg_i != -1) {
		printf("\nError: One argument expected for mode 'manual', got two\n\n");
		printf(HELP_MSG);
		return 1;
	}
	const char* output_file = output_file_arg_i == -1 ? DEFAULT_OUTPUT_FILE : argv[output_file_arg_i];
	const char* player1_name = player1_name_arg_i == -1 ? DEFAULT_PLAYER1_NAME : argv[player1_name_arg_i];
	const char* player2_name = player2_name_arg_i == -1 ? DEFAULT_PLAYER2_NAME : argv[player2_name_arg_i];





	char* turn_data = malloc(2 * roundc * sizeof(char));
	if (turn_data == NULL) {
		printf("Failed to allocate memory for player turns (%i bytes), exiting.", 2 * roundc);
		return 1;
	}

	if (mode == 'm') {

		file = fopen(argv[filename_arg_i], "r");
		compile(file, memory, bytecode);

		#ifdef DEBUG
			int i = 0;
			while (bytecode[i] != INST_NULL) { i++; };
			dump_bytecode(bytecode, i);
		#endif

		printf("Teraz mozes hrat proti tvojmu botovi. Napis na vstup znak `k`, `p` alebo `n` pre zahranie tahu, 'e' pre vzdanie sa alebo `!` pre ukončenie hry.\n");

		/* na zaciatku sa v predoslom kole nehralo nic, takze to chcem byt nieco ine jak 0,1,2 */
		memory[ADDRESS_OPPONENTS_LAST_PLAY] = -1;
		int round_i;
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
				printf("Neznamy tah `%c`, platne tahy su: `k`, `p` alebo `n` pre zahranie tahu, 'e' pre vzdanie sa alebo `!` pre ukoncenie hry.\n", play_char); 
				round_i--; // Nepocitajme to ako tah
				continue; 
			}

			bot_play = run(memory, bytecode);
			if (bot_play == -1){
				printf("Bot nezahral tah, koniec hry.\n");
				break;
			}
			printf("ty: %7s,  bot: %7s\n", play_names[play], play_names[bot_play]);
			memory[ADDRESS_OPPONENTS_LAST_PLAY] = play;
			turn_data[round_i * 2] = play;
			turn_data[round_i * 2 + 1] = bot_play;

			if (play == TURN_ERROR || bot_play == TURN_ERROR) 
				break;
		}
		if (round_i == roundc) printf("\nVsetky kola boli zahrate (%d), koniec\n", round_i);
		else printf("\nHra bola ukoncena predcasne v kole %d\n", round_i);

		printf("Saving game to `%s`...", output_file);
		/* Poznamka: Pouzivame round_i namiesto roundc, lebo ked ukoncime hru predcasne mame len round_i odohranych kol */
		save_gamedata(player1_name, player2_name, round_i, turn_data, output_file);
		
		#ifdef DEBUG
			printf("\n GAME LOG:\n");
			for (int i = 0; i < round_i; i++) {
				printf("TURN %i: human: %s, bot: %s\n", i, play_names[turn_data[i*2]], play_names[turn_data[i*2+1]]);
			}
		#endif

	} else if (mode == 'p') {
		printf("Not implemented");
		free(turn_data);
		return 1;
	} else {
		printf("Unknown mode: `%c`", mode);
		free(turn_data);
		return 1;
	}
	free(turn_data);
	return 0;
}