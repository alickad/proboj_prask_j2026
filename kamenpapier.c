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
	if (c < 'a' || c > 'z') {
		success = 0;
		goto end;
	}
	for (i = 0; i < MAX_VARIABLE_LENGTH; i++) {
		*dest = c;
		dest++;
		c = fgetc(reader->stream);
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) goto end;
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
	if (c >= 'a' && c <= 'z') goto fail;
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
	if (c >= 'a' && c <= 'z') {
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
		else if (read_char(reader, '=')) comparison = INST_EQUAL;
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
	while (read_keyword(&reader, "nech")) {
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
			case INST_PLAY_ROCK:      return 0;
			case INST_PLAY_PAPER:     return 1;
			case INST_PLAY_SCISSORS:  return 2;
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

int main(int argc, char **argv) {
	FILE *file;
	int memory[MEMORY_SIZE];
	unsigned char bytecode[MAX_BYTECODE_LENGTH];

	int play;
	char play_char;
	int bot_play;

	const char *play_names[3] = {"kamen", "papier", "noznice"};

	if (argc != 2) {
		printf("usage: kamenpapier <subor>\n");
	}
	file = fopen(argv[1], "r");
	compile(file, memory, bytecode);

	/*dump_bytecode(bytecode, length);*/

	printf("Teraz mozes hrat proti tvojmu botovi. Napis na vstup znak `k`, `p` alebo `n`.\n");

	/* na zaciatku sa v predoslom kole nehralo nic, takze to chcem byt nieco ine jak 0,1,2 */
	memory[ADDRESS_OPPONENTS_LAST_PLAY] = -1;
	while (scanf("%c", &play_char) > 0) {
		if (play_char == 'k') play = 0;
		else if (play_char == 'p') play = 1;
		else if (play_char == 'n') play = 2;
		else continue;

		bot_play = run(memory, bytecode);
		printf("ty: %7s,  bot: %7s\n", play_names[play], play_names[bot_play]);
		memory[ADDRESS_OPPONENTS_LAST_PLAY] = play;
	}
}