#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

typedef enum TokenType
{
    TYPE_NULL,
    TYPE_DICE,
    TYPE_ADV,
    TYPE_OPERATOR,
    TYPE_UNARY,
    TYPE_CONSTANT,
    TYPE_LIST,
    TYPE_LOOP,
    TYPE_IF
} TokenType;

typedef struct Token
{
    // General Stuff
    TokenType type;
    int value;

    // General Stuff (OG)
    TokenType ogType;
    int ogValue;

    // Specific for Lists/List-Items
    struct Token *parent;
    struct Token *children;
    int childCount;

    // Specific for Dice
    int maxValue;
    int result;
    int *results;
    int resultCount;
    int validResultCount;
} Token;

typedef struct Var
{
    char *name;
    int value;
} Var;

typedef struct Macro
{
    char *name;
    char *value;

    char **parameters;
    int paramCount;
} Macro;

typedef enum CONFIGS
{
    DEBUG_MESSAGES,
    SHOW_LOADED,
    FULL_DICES,
    FULL_LISTS,
    FULL_LOOPS,
    CONFIG_COUNT
} CONFIGS;

const char *GLOBAL_NAMES[CONFIG_COUNT] = {"DEBUG_MESSAGES", "SHOW_LOADED", "FULL_DICES", "FULL_LISTS", "FULL_LOOPS"};
bool GLOBAL_CONFIGS[CONFIG_COUNT];

const int opPriority[256] = {
    ['|'] = 0,
    ['&'] = 1,
    ['='] = 2,
    ['!'] = 2,
    ['>'] = 3,
    ['<'] = 3,
    ['s'] = 3,
    ['b'] = 3,
    ['+'] = 4,
    ['-'] = 4,
    ['*'] = 5,
    ['/'] = 5,
    ['%'] = 5,
    ['^'] = 6};

Token tokens;
Token *currentToken;

Var *vars = NULL;
Macro *macros = NULL;
int varCount;
int macroCount;
int spacing;

const char *configPath = "./ydice.config";

// --- FUNCTIONS ---
int Tokenizer(char *str);
int Calculate(Token *parent, int id, int op);
int Error(const char *errorType, const char *error, const char *extra)
{
    printf("\x1b[91m");
    if (extra[0] == '\0')
        printf("%s ERROR: %s\n", errorType, error);
    else
        printf("%s ERROR: %s[%s]\n", errorType, error, extra);
    printf("\x1b[0m");
    return -1;
}

void PrintMessage(const char *format, ...)
{
    if (!GLOBAL_CONFIGS[DEBUG_MESSAGES])
        return;

    for (int i = 0; i < spacing; i++)
        printf("  ");

    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}

const char *GetTypeString(TokenType type)
{
    switch (type)
    {
    case TYPE_DICE:
        return "Dice";
    case TYPE_ADV:
        return "Advantage";
    case TYPE_OPERATOR:
        return "Binary Operator";
    case TYPE_UNARY:
        return "Unary Operator";
    case TYPE_CONSTANT:
        return "Constant";
    case TYPE_LIST:
        return "List";
    case TYPE_LOOP:
        return "Loop";
    default:
        return "NULL";
    }
}

bool AllowedChar(char c)
{
    char *notAllowed = " ~$@|&!=.,;(){}[]+-*/^%\n\0";
    return !strchr(notAllowed, c);
}

int AdvCompare(const void *a, const void *b)
{
    int int_a = *((int *)a);
    int int_b = *((int *)b);

    if (int_a > int_b)
        return -1;
    if (int_a < int_b)
        return 1;
    return 0;
}

int DsvCompare(const void *a, const void *b)
{
    int int_a = *((int *)a);
    int int_b = *((int *)b);

    if (int_a < int_b)
        return -1;
    if (int_a > int_b)
        return 1;
    return 0;
}

void AddToken(TokenType type, int value)
{
    Token token;
    token.type = type;
    token.value = value;
    token.ogType = type;
    token.ogValue = value;

    token.parent = currentToken;
    token.children = NULL;
    token.results = NULL;

    token.maxValue = 0;
    token.result = 0;
    token.resultCount = 0;
    token.validResultCount = 0;
    token.childCount = 0;

    currentToken->childCount++;
    currentToken->children = (Token *)realloc(currentToken->children, sizeof(Token) * currentToken->childCount);
    currentToken->children[currentToken->childCount - 1] = token;

    PrintMessage("ADDED <%s><%d>\n", GetTypeString(type), value);
    Token *tokenRef = &currentToken->children[currentToken->childCount - 1];
    if (type == TYPE_LIST)
    {
        tokenRef->children = (Token *)malloc(sizeof(Token));
        currentToken = tokenRef;
    }
    else if (type == TYPE_DICE)
        tokenRef->results = (int *)malloc(sizeof(int));
}

int AddVar(char *id)
{
    Var *var = NULL;
    for (int i = 0; i < varCount; i++)
    {
        if (!strcmp(id, vars[i].name))
        {
            var = &vars[i];
            break;
        }
    }

    if (var == NULL)
        return Error("VAR", "No Var named ", id);

    PrintMessage("VAR <%s> ADDED AS <%d>\n", id, var->value);
    AddToken(TYPE_CONSTANT, var->value);
}

int AddMacro(char *id, char **parameters, int paramCount)
{
    Macro *macro = NULL;
    for (int i = 0; i < macroCount; i++)
    {
        if (!strcmp(id, macros[i].name))
        {
            macro = &macros[i];
            break;
        }
    }

    if (macro == NULL)
        return Error("MACRO", "No Macro named ", id);

    if (paramCount < macro->paramCount)
        return Error("MACRO", "Not Enough Parameters for Macro ", id);
    if (paramCount > macro->paramCount)
        return Error("MACRO", "Too Many Parameters for Macro ", id);

    if (GLOBAL_CONFIGS[DEBUG_MESSAGES])
    {
        PrintMessage("MACRO <%s><", id);
        for (int i = 0; i < paramCount; i++)
        {
            if (i)
                printf(", ");
            printf("%s", parameters[i]);
        }
        printf("> ADDED AS <%s>\n", macro->value);
    }

    char *fixedStr;
    char *oldStr = macro->value;
    if (paramCount > 0)
    {
        for (int p = 0; p < paramCount; p++)
        {
            int paramLen = strlen(macro->parameters[p]) + 1;
            int valueLen = strlen(parameters[p]);

            char *fixedParameter = (char *)malloc(sizeof(char) * (paramLen + 1));
            fixedParameter[paramLen] = '\0';
            fixedParameter[0] = '$';
            for (int j = 0; j < paramLen - 1; j++)
                fixedParameter[j + 1] = macro->parameters[p][j];

            int i, count = 0;
            for (i = 0; oldStr[i] != '\0';)
            {
                if (strstr(&oldStr[i], fixedParameter) == &oldStr[i])
                {
                    count++;
                    i += paramLen;
                }
                else
                    i++;
            }
            fixedStr = (char *)malloc(sizeof(char) * (i + count * (valueLen - paramLen) + 1));

            i = 0;
            while (*oldStr)
            {
                if (strstr(oldStr, fixedParameter) == oldStr)
                {
                    strcpy(&fixedStr[i], parameters[p]);
                    i += valueLen;
                    oldStr += paramLen;
                }
                else
                    fixedStr[i++] = *oldStr++;
            }
            fixedStr[i] = '\0';
            oldStr = fixedStr;
        }
    }
    else
    {
        int length = strlen(oldStr);
        fixedStr = (char *)malloc(sizeof(char) * (length + 1));
        strcpy(fixedStr, oldStr);
    }

    spacing++;
    PrintMessage("FIXED MACRO <%s>\n", fixedStr);
    spacing--;
    Tokenizer(fixedStr);
}

void AddNewVar(char *name, int value)
{
    Var newVar;
    newVar.value = value;
    newVar.name = (char *)malloc(sizeof(char) * strlen(name));
    strcpy(newVar.name, name);

    for (int i = 0; i < varCount; i++)
    {
        if (!strcmp(name, vars[i].name))
        {
            vars[i] = newVar;
            return;
        }
    }

    varCount++;
    vars = (Var *)realloc(vars, sizeof(Var) * varCount);
    vars[varCount - 1] = newVar;
}

void AddNewMacro(char *name, char *value, int paramCount, char **params)
{
    Macro newMacro;
    newMacro.name = (char *)malloc(sizeof(char) * strlen(name));
    strcpy(newMacro.name, name);

    newMacro.value = (char *)malloc(sizeof(char) * strlen(value));
    strcpy(newMacro.value, value);
    newMacro.paramCount = paramCount;

    newMacro.parameters = (char **)malloc(sizeof(char *) * paramCount);
    for (int i = 0; i < paramCount; i++)
    {
        char *param = params[i];
        newMacro.parameters[i] = (char *)malloc(sizeof(char) * strlen(param));
        strcpy(newMacro.parameters[i], param);
    }

    for (int i = 0; i < macroCount; i++)
    {
        if (!strcmp(name, macros[i].name))
        {
            macros[i] = newMacro;
            return;
        }
    }

    macroCount++;
    macros = (Macro *)realloc(macros, sizeof(Macro) * macroCount);
    macros[macroCount - 1] = newMacro;
}

void CopyChildren(Token *source, Token *dest)
{
    if (source->childCount <= 0)
        return;

    dest->children = (Token *)malloc(sizeof(Token) * source->childCount);
    memcpy(dest->children, source->children, sizeof(Token) * source->childCount);

    for (int i = 0; i < source->childCount; i++)
        CopyChildren(&source->children[i], &dest->children[i]);
}

void Init()
{
    struct timespec spec;
    timespec_get(&spec, TIME_UTC);
    srand(spec.tv_nsec);

    tokens.type = TYPE_NULL;
    tokens.value = 0;
    tokens.children = (Token *)malloc(sizeof(Token));
    tokens.childCount = 0;
    tokens.parent = NULL;
    currentToken = &tokens;
}

int Tokenizer(char *str)
{
    int strlength = strlen(str);
    int counter = 0;
    int num = 0;
    bool numNull = true;

    for (int i = 0; i < strlength; i++)
    {
        char c = str[i];
        switch (c)
        {
        // Arithmetic Operators
        case '-':
            if (numNull && (currentToken->childCount == 0 || currentToken->children[currentToken->childCount - 1].type == TYPE_OPERATOR))
            {
                if (currentToken->childCount == 0)
                    printf("\nCC: [%c]\n\n");
                else
                    printf("\nTYPE: [%s]\n\n", GetTypeString(currentToken->children[currentToken->childCount - 1].type));

                AddToken(TYPE_UNARY, c);
                break;
            }
        case '+':
        case '*':
        case '/':
        case '%':
        case '^':
            if (!numNull)
            {
                AddToken(TYPE_CONSTANT, num);
                num = 0;
                numNull = true;
            }
            AddToken(TYPE_OPERATOR, c);
            break;

        // Logic Operators
        case '>':
        case '<':
        case '=':
        case '!':
            if (!numNull)
            {
                AddToken(TYPE_CONSTANT, num);
                num = 0;
                numNull = true;
            }

            if (i + 1 < strlength && str[i + 1] == '=')
            {
                if (c == '>')
                    AddToken(TYPE_OPERATOR, 'b');
                else if (c == '<')
                    AddToken(TYPE_OPERATOR, 's');
                else
                    AddToken(TYPE_OPERATOR, c);

                i++;
            }
            else if (c == '!')
                AddToken(TYPE_UNARY, c);
            else
                AddToken(TYPE_OPERATOR, c);

            break;

        // OR & AND
        case '|':
        case '&':
            if (!numNull)
            {
                AddToken(TYPE_CONSTANT, num);
                num = 0;
                numNull = true;
            }

            if (i + 1 < strlength && str[i + 1] == c)
                i++;

            AddToken(TYPE_OPERATOR, c);
            break;

        // Advantage
        case 'v':
            if (!numNull)
            {
                AddToken(TYPE_CONSTANT, num);
                num = 0;
                numNull = true;
            }
            AddToken(TYPE_ADV, 1);
            break;

        // Dice and Disadvantage
        case 'd':
            if (!numNull)
            {
                AddToken(TYPE_CONSTANT, num);
                num = 0;
            }

            if (i + 1 < strlength && str[i + 1] == 'v')
            {
                AddToken(TYPE_ADV, 0);
                i++;
            }
            else
                AddToken(TYPE_DICE, 0);

            break;

        // Lists
        case '(':
            AddToken(TYPE_LIST, 0);
            PrintMessage("LIST START\n");
            spacing++;
            if (counter)
                counter++;
            break;

        case ')':
            if (!numNull)
            {
                AddToken(TYPE_CONSTANT, num);
                num = 0;
                numNull = true;
            }
            currentToken = currentToken->parent;
            spacing--;
            PrintMessage("LIST END\n");

            if (counter == 2)
            {
                counter = 0;
                currentToken = currentToken->parent;
                spacing--;
                PrintMessage("LOOP LIST END\n");
            }
            else if (counter)
                counter--;
            break;

        // Vars
        case '$':
            int v = 0;
            while (i + v < strlength)
            {
                v++;
                if (!AllowedChar(str[i + v]))
                    break;
            }
            if (v == 1)
                return Error("VAR", "Var Name is Invalid/Too Short.", "");

            char *varName = (char *)malloc(sizeof(char) * v);
            strncpy(varName, str + i + 1, v - 1);
            varName[v - 1] = '\0';
            AddVar(varName);
            i += v - 1;
            break;

        // Macros
        case '@':
            int m = 0;
            while (i + m < strlength)
            {
                m++;
                if (!AllowedChar(str[i + m]))
                    break;
            }
            if (m == 1)
                return Error("MACRO", "Macro Name is Invalid/Too Short.", "");

            char *macroName = (char *)malloc(sizeof(char) * m);
            strncpy(macroName, str + i + 1, m - 1);
            macroName[m - 1] = '\0';

            i += m;
            int paramCount = 0;
            char **parameters = (char **)malloc(sizeof(char *));
            if (i + 1 < strlength && str[i] == '(' && str[i + 1] != ')')
            {
                bool done = false;
                while (!done)
                {
                    int p = 0;
                    while (true)
                    {
                        if (i + p >= strlength)
                            return Error("MACRO", "Macro Parameters are Unfinished", "");

                        p++;
                        if (str[i + p] == ',' || str[i + p] == ')')
                        {
                            done = str[i + p] == ')';
                            break;
                        }
                    }
                    if (p == 1)
                        return Error("MACRO", "Macro Parameter Name is Invalid/Too Short.", "");

                    parameters = (char **)realloc(parameters, sizeof(char *) * (paramCount + 1));
                    parameters[paramCount] = (char *)malloc(sizeof(char) * p);
                    strncpy(parameters[paramCount], str + i + 1, p - 1);
                    parameters[paramCount][p - 1] = '\0';
                    paramCount++;
                    i += p;
                }
            }

            AddMacro(macroName, parameters, paramCount);
            break;

        // Loop
        case 'l':
            if (i + 1 >= strlength || str[i + 1] != 'o')
                break;
            if (i + 2 >= strlength || str[i + 2] != 'o')
                break;
            if (i + 3 >= strlength || str[i + 3] != 'p')
                break;

            i += 3;
            counter += 1;
            AddToken(TYPE_LOOP, 0);
            AddToken(TYPE_LIST, 0);
            PrintMessage("LOOP LIST START\n");
            spacing++;
            break;

        case 'i':
            if (i + 1 >= strlength || str[i + 1] != 'f')
                break;

            i += 1;
            counter += 1;
            AddToken(TYPE_IF, 0);
            AddToken(TYPE_LIST, 0);
            PrintMessage("IF LIST START\n");
            spacing++;
            break;

        // Separator
        case ',':
            if (!numNull)
            {
                AddToken(TYPE_CONSTANT, num);
                num = 0;
                numNull = true;
            }
            currentToken = currentToken->parent;
            spacing--;
            PrintMessage("LIST END\n");
            AddToken(TYPE_LIST, 0);
            PrintMessage("LIST START\n");
            spacing++;
            break;

        // Number Counting
        default:
            if (c >= '0' && c <= '9')
            {
                num = num * 10 + c - '0';
                numNull = false;
            }

            if (i == strlength - 1 && !numNull)
            {
                AddToken(TYPE_CONSTANT, num);
                num = 0;
            }
            break;
        }
    }
}

int CalculateDice(Token *parent, int id)
{
    if (id >= parent->childCount)
        return -1;

    Token *current = &parent->children[id];
    if (current->type != TYPE_DICE)
        return Error("DICE", "This is NOT a dice HOW are you CALCULATING A DICE THAT IS NOT A DICE.", "");

    Token *nextToken = NULL;
    if (id + 1 < parent->childCount)
        nextToken = &parent->children[id + 1];
    else
        return Error("DICE", "Next Token is Null.", "");

    int previousVal = 0;
    int nextVal = 0;
    if (id >= 1 && (parent->children[id - 1].type == TYPE_CONSTANT || parent->children[id - 1].type == TYPE_LIST))
    {
        if (parent->children[id - 1].type == TYPE_LIST)
            Calculate(parent, id - 1, -1);

        previousVal = parent->children[id - 1].value;
    }
    else
        previousVal = 1;

    if (nextToken->type == TYPE_CONSTANT)
        nextVal = nextToken->value;
    else if (nextToken->type == TYPE_DICE)
    {
        CalculateDice(parent, id + 1);
        nextToken->type = TYPE_CONSTANT;
        nextToken->value = nextToken->result;
        nextVal = nextToken->value;
    }
    else if (nextToken->type == TYPE_LIST || nextToken->type == TYPE_LOOP)
    {
        Calculate(nextToken, 0, 0);
        nextVal = nextToken->value;
    }
    else
        return Error("DICE", "Next Token is not of Constant, List or Dice Types. ", GetTypeString(nextToken->type));

    current->maxValue = nextVal;
    current->resultCount = previousVal;
    current->results = (int *)malloc(sizeof(int) * current->resultCount);
    for (int i = 0; i < previousVal; i++)
        current->results[i] = (rand() % nextVal) + 1;

    int newID = id;
    char textMod[3] = "";
    char textModNum[256] = "";
    if (id + 2 < parent->childCount && parent->children[id + 2].type == TYPE_ADV)
    {
        if (parent->children[id + 2].value)
        {
            qsort(current->results, previousVal, sizeof(int), AdvCompare);
            textMod[0] = 'v';
            textMod[1] = '\0';
        }
        else
        {
            qsort(current->results, previousVal, sizeof(int), DsvCompare);
            textMod[0] = 'd';
            textMod[1] = 'v';
            textMod[2] = '\0';
        }

        if (id + 3 < parent->childCount && parent->children[id + 3].type == TYPE_CONSTANT)
        {
            current->validResultCount = parent->children[id + 3].value;
            for (int i = 0; i < current->validResultCount; i++)
                current->result += current->results[i];

            snprintf(textModNum, 256, "%d", current->validResultCount);

            newID += 3;
            parent->children[newID].type = TYPE_CONSTANT;
            parent->children[newID].value = current->result;
        }
        else
        {
            current->validResultCount = 1;
            current->result = current->results[0];

            textModNum[0] = 1;
            textModNum[1] = '\0';

            newID += 2;
            parent->children[newID].type = TYPE_CONSTANT;
            parent->children[newID].value = current->result;
        }
    }
    else
    {
        current->validResultCount = current->resultCount;
        for (int i = 0; i < previousVal; i++)
            current->result += current->results[i];

        newID++;
        nextToken->type = TYPE_CONSTANT;
        nextToken->value = current->result;
    }

    PrintMessage("ROLLED %dd%d%s%s | RESULTS: %d\n", previousVal, nextVal, textMod, textModNum, current->result);
    return newID;
}

int Calculate(Token *parent, int id, int op)
{
    if (id >= parent->childCount)
        return -1;

    int newID = id;
    Token *current = &parent->children[id];
    Token *previous = NULL;
    Token *next = NULL;
    Token *nextNext = NULL;
    if (id >= 1)
        previous = &parent->children[id - 1];
    if (id + 1 < parent->childCount)
    {
        next = &parent->children[id + 1];
        if (id + 2 < parent->childCount)
            nextNext = &parent->children[id + 2];
    }

    switch (current->type)
    {
    case TYPE_OPERATOR:
        if (next == NULL)
            return Error("OPERATOR", "Next Token is Null.", "");
        if (previous == NULL)
            return Error("OPERATOR", "Previous Token is Null.", "");
        if (previous->type != TYPE_CONSTANT)
            return Error("OPERATOR", "Previous Token is not of Constant Type. ", GetTypeString(previous->type));

        if (op != 0 && opPriority[current->value] <= op)
            return newID - 1;

        int previousVal = previous->value;
        int nextVal = 0;
        bool updateNextNext = false;
        if (next->type == TYPE_UNARY)
        {
            newID = Calculate(parent, id + 1, opPriority[current->value]);
            nextVal = parent->children[newID].value;
        }
        else if (nextNext != NULL && nextNext->type == TYPE_DICE)
        {
            newID = CalculateDice(parent, id + 2);
            nextNext->type = TYPE_CONSTANT;
            nextNext->value = nextNext->result;
            nextVal = nextNext->value;
        }
        else if (nextNext != NULL && nextNext->type == TYPE_OPERATOR && opPriority[nextNext->value] > opPriority[current->value])
        {
            newID = Calculate(parent, id + 2, opPriority[current->value]);
            nextVal = parent->children[newID].value;
        }
        else if (next->type == TYPE_DICE)
        {
            newID = CalculateDice(parent, id + 1);
            next->type = TYPE_CONSTANT;
            next->value = next->result;
            nextVal = next->value;
        }
        else if (next->type == TYPE_LIST)
        {
            Calculate(next, 0, 0);
            nextVal = next->value;
        }
        else if (next->type == TYPE_LOOP)
        {
            Calculate(parent, id + 1, 0);
            nextVal = next->value;
            if (nextNext != NULL)
                updateNextNext = true;
        }
        else if (next->type == TYPE_CONSTANT)
            nextVal = next->value;
        else
            Error("OPERATOR", "Next Token is not of Constant, List or Dice Types. ", GetTypeString(next->type));

        int finalVal = 0;
        switch (current->value)
        {
        case '+':
            finalVal = previousVal + nextVal;
            break;

        case '-':
            finalVal = previousVal - nextVal;
            break;

        case '*':
            finalVal = previousVal * nextVal;
            break;

        case '/':
            finalVal = previousVal / nextVal;
            break;

        case '%':
            finalVal = previousVal % nextVal;
            break;

        case '^':
            finalVal = powf((float)previousVal, (float)nextVal);
            break;

        case '>':
            finalVal = previousVal > nextVal;
            break;

        case '<':
            finalVal = previousVal < nextVal;
            break;

        case 'b':
            finalVal = previousVal >= nextVal;
            break;

        case 's':
            finalVal = previousVal <= nextVal;
            break;

        case '=':
            finalVal = previousVal == nextVal;
            break;

        case '!':
            finalVal = previousVal != nextVal;
            break;

        case '|':
            finalVal = previousVal || nextVal;
            break;

        case '&':
            finalVal = previousVal && nextVal;
            break;

        default:
            return Error("OPERATOR", "Operator is Unknown", "");
        }

        PrintMessage("CALCULATED %d %c %d | RESULT: %d\n", previousVal, current->value, nextVal, finalVal);
        int idOff = (newID == id);
        parent->children[newID + idOff].value = finalVal;
        if (updateNextNext)
            nextNext->value = finalVal;

        if (newID + 1 + idOff < parent->childCount)
        {
            if (op != 0)
                return Calculate(parent, newID + 1 + idOff, op);
            else
                Calculate(parent, newID + 1 + idOff, 0);
        }
        else
        {
            parent->value = finalVal;
            parent->type = TYPE_CONSTANT;
        }
        return newID + idOff;

    case TYPE_UNARY:
        if (next == NULL)
            return Error("UNARY OPERATOR", "Next Token is Null.", "");

        int nextUnVal = 0;
        bool updateNextNextUn = false;
        if (nextNext != NULL && nextNext->type == TYPE_DICE)
        {
            newID = CalculateDice(parent, id + 2);
            nextNext->type = TYPE_CONSTANT;
            nextNext->value = nextNext->result;
            nextUnVal = nextNext->value;
        }
        else if (next->type == TYPE_DICE)
        {
            newID = CalculateDice(parent, id + 1);
            next->type = TYPE_CONSTANT;
            next->value = next->result;
            nextUnVal = next->value;
        }
        else if (next->type == TYPE_LIST)
        {
            Calculate(next, 0, 0);
            nextUnVal = next->value;
        }
        else if (next->type == TYPE_LOOP)
        {
            Calculate(parent, id + 1, 0);
            nextUnVal = next->value;
            if (nextNext != NULL)
                updateNextNextUn = true;
        }
        else if (next->type == TYPE_CONSTANT)
            nextUnVal = next->value;
        else
            Error("UNARY OPERATOR", "Next Token is not of Constant, List or Dice Types. ", GetTypeString(next->type));

        int finalUnVal = 0;
        switch (current->value)
        {
        case '-':
            finalUnVal = -nextUnVal;
            break;

        case '!':
            finalUnVal = !nextUnVal;
            break;

        default:
            return Error("UNARY OPERATOR", "Operator is Unknown", "");
        }

        PrintMessage("CALCULATED %c%d | RESULT: %d\n", current->value, nextUnVal, finalUnVal);
        int idOffUn = (newID == id);
        parent->children[newID + idOffUn].value = finalUnVal;
        if (updateNextNextUn)
            nextNext->value = finalUnVal;

        if (newID + 1 + idOffUn >= parent->childCount)
        {
            parent->value = finalVal;
            parent->type = TYPE_CONSTANT;
        }
        else if (nextNext != NULL && nextNext->type == TYPE_OPERATOR && opPriority[nextNext->value] > op)
            return Calculate(parent, id + 2, op);
        else if (previous == NULL)
            Calculate(parent, newID + 1 + idOffUn, 0);

        return newID + idOffUn;

    case TYPE_DICE:
        newID = CalculateDice(parent, id);
        if (newID == -1)
            return -1;

        Calculate(parent, newID, 0);
        return newID;

    case TYPE_LIST:
        Calculate(current, 0, 0);
        if (op >= 0)
            Calculate(parent, id, 0);
        return id;

    case TYPE_CONSTANT:
        if (next != NULL)
            Calculate(parent, id + 1, 0);
        else
        {
            parent->value = current->value;
            parent->type = TYPE_CONSTANT;
        }
        return id;

    case TYPE_LOOP:
        PrintMessage("-LOOP START\n");
        spacing++;
        if (next == NULL)
            return Error("LOOP", "Next Token is Null.", "");
        if (next->type != TYPE_LIST)
            return Error("LOOP", "Next Token Type is not List. ", GetTypeString(next->type));
        if (next->childCount != 2)
            return Error("LOOP", "Next List Token does not have EXACTLY 2 Children.", "");
        if (next->children[0].type != TYPE_LIST || next->children[1].type != TYPE_LIST)
            return Error("LOOP", "One of the List children is not of the List Type.", "");

        Calculate(&next->children[0], 0, 0);
        int loopCount = next->children[0].value;
        int loopResult = 0;
        if (loopCount > 1)
        {
            next->children = (Token *)realloc(next->children, sizeof(Token) * (loopCount + 1));
            next->childCount = loopCount + 1;
            for (int l = 2; l < loopCount + 1; l++)
            {
                spacing--;
                PrintMessage("-ITERATION %d\n", l - 1);
                spacing++;
                next->children[l] = next->children[1];
                CopyChildren(&next->children[1], &next->children[l]);
                Calculate(&next->children[l], 0, 0);
            }
            spacing--;
            PrintMessage("-FINAL ITERATION\n");
            spacing++;
            Calculate(&next->children[1], 0, 0);

            for (int l = 1; l < loopCount + 1; l++)
                loopResult += next->children[l].value;
        }
        else
        {
            Calculate(&next->children[1], 0, 0);
            loopResult = next->children[1].value;
        }

        spacing--;
        PrintMessage("-LOOP END | RESULTS: %d\n", loopResult);
        current->value = loopResult;
        next->value = loopResult;
        next->type = TYPE_CONSTANT;
        if (id + 2 < parent->childCount)
            Calculate(parent, id + 2, 0);
        else
        {
            parent->value = current->value;
            parent->type = TYPE_CONSTANT;
        }
        break;

    case TYPE_IF:
        PrintMessage("-IF START\n");
        spacing++;
        if (next == NULL)
            return Error("IF", "Next Token is Null.", "");
        if (next->type != TYPE_LIST)
            return Error("IF", "Next Token Type is not List. ", GetTypeString(next->type));
        if (next->childCount != 2 && next->childCount != 3)
            return Error("IF", "Next List Token does not have 2 or 3 Children.", "");
        if (next->children[0].type != TYPE_LIST || next->children[1].type != TYPE_LIST || (next->childCount == 3 && next->children[2].type != TYPE_LIST))
            return Error("IF", "One of the List children is not of the List Type.", "");

        Calculate(&next->children[0], 0, 0);
        bool conditional = next->children[0].value;
        int ifResult = 0;
        if (conditional)
        {
            Calculate(&next->children[1], 0, 0);
            ifResult = next->children[1].value;
        }
        else if (next->childCount == 3)
        {
            Calculate(&next->children[2], 0, 0);
            ifResult = next->children[2].value;
        }

        spacing--;
        PrintMessage("-IF END | RESULTS: %d\n", ifResult);
        current->value = ifResult;
        next->value = ifResult;
        next->type = TYPE_CONSTANT;
        if (id + 2 < parent->childCount)
            Calculate(parent, id + 2, 0);
        else
        {
            parent->value = current->value;
            parent->type = TYPE_CONSTANT;
        }

        break;

    default:
        return Error("CALCULATOR", "Invalid Token Type. ", GetTypeString(current->type));
    }
}

void PrintResults(Token *parent, int type)
{
    bool typeThing = false;
    for (int c = 0; c < parent->childCount; c++)
    {
        Token *current = &parent->children[c];
        Token *next = NULL;
        switch (current->ogType)
        {
        case TYPE_DICE:
            int off = 0;
            bool skip = false;
            while (c + off < parent->childCount)
            {
                off++;
                if (parent->children[c + off].type == TYPE_OPERATOR || parent->children[c + off].type == TYPE_DICE)
                {
                    skip = parent->children[c + off].type == TYPE_DICE;
                    break;
                }
            }
            c += off - 1;
            if (skip)
                continue;

            if (GLOBAL_CONFIGS[FULL_DICES])
            {
                printf("[");
                for (int i = 0; i < current->resultCount; i++)
                {
                    if (i >= current->validResultCount)
                        printf("\x1b[90m");
                    else if (current->results[i] == 1)
                        printf("\x1b[91m");
                    else if (current->results[i] == current->maxValue)
                        printf("\x1b[92m");

                    printf(" %d \x1b[0m", current->results[i]);
                }
                printf("]");
            }
            else
                printf("[ %d ]", current->result);

            break;

        case TYPE_OPERATOR:
            printf(" %c ", current->ogValue);
            break;

        case TYPE_UNARY:
            printf("%c", current->ogValue);
            break;

        case TYPE_CONSTANT:
            if (c + 1 < parent->childCount && parent->children[c + 1].ogType == TYPE_DICE)
                break;

            printf("%d", current->ogValue);
            break;

        case TYPE_LIST:
            if (c + 1 < parent->childCount && parent->children[c + 1].ogType == TYPE_DICE)
                break;

            if (type == 2 && !typeThing)
            {
                typeThing = true;
                break;
            }

            if (GLOBAL_CONFIGS[FULL_LISTS])
            {
                printf("(");
                PrintResults(current, 1);
                printf(")");
            }
            else
                printf("(%d)", current->value);

            break;

        case TYPE_LOOP:
            c++;
            if (c < parent->childCount)
                next = &parent->children[c];
            else
                break;

            if (GLOBAL_CONFIGS[FULL_LOOPS])
            {
                printf("{");
                for (int i = 1; i < next->childCount; i++)
                {
                    printf("(");
                    PrintResults(&next->children[i], 2);
                    printf(")");
                    if (i + 1 < next->childCount)
                        printf(" + ");
                }
                printf("}");
            }
            else
                printf("{ %d }", current->value);

            break;

        default:
            break;
        }
    }

    if (!type)
        printf("\nFINAL RESULT: [%d]\n", tokens.value);
}

int Program(char *str)
{
    Init();

    spacing = 0;
    PrintMessage("\nSTARTING TOKENIZATION...\n");
    spacing++;
    Tokenizer(str);

    spacing = 0;
    PrintMessage("\nSTARTING CALCULATION...\n");
    spacing++;
    Calculate(&tokens, 0, 0);

    spacing = 0;
    PrintMessage("\nRESULTS:\n");
    spacing++;
    PrintResults(&tokens, 0);

    return 0;
}

void LoadConfig()
{
    FILE *configFile = fopen(configPath, "r");
    if (configFile == NULL)
        return;

    fseek(configFile, 0, SEEK_END);
    int fileSize = ftell(configFile);
    rewind(configFile);

    char *file = (char *)malloc(sizeof(char) * (fileSize + 1));
    size_t bytesRead = fread(file, 1, fileSize, configFile);
    file[bytesRead] = '\0';
    fclose(configFile);

    file--;
    char cBuffer[0xffff];
    char cBuffer2[0xffff];
    char *sBuffer[0xff];
    for (int i = 0; i < 0xff; i++)
        sBuffer[i] = (char *)malloc(sizeof(char) * 0xffff);

    int iBuffer = 0;
    int mode = 0; // Null[0] Configs[1] Vars[2] Macros[3];
    for (int i = 0; i < bytesRead; i++)
    {
        file++;
        char c = *file;

        switch (c)
        {
        case '~':
            mode++;
        case '\n':
            continue;

        default:
            break;
        }

        switch (mode)
        {
        case 1: // CONFIGS
            for (int cID = 0; cID < CONFIG_COUNT; cID++)
            {
                const char *globalName = GLOBAL_NAMES[cID];
                int globalLen = strlen(globalName);
                if (strncmp(file, globalName, globalLen) == 0)
                {
                    file += globalLen + 1;
                    GLOBAL_CONFIGS[cID] = *file - '0';
                    break;
                }
            }
            break;

        case 2: // VARS
            if (c == '$')
                file++;

            c = *file;
            while (c != ' ' && c != '\n' && c != '\0')
            {
                cBuffer[iBuffer] = c;
                iBuffer++;
                file++;
                c = *file;
            }
            cBuffer[iBuffer] = '\0';
            iBuffer = 0;

            file++;
            c = *file;
            int mult = 1;
            if (c == '-')
            {
                file++;
                c = *file;
                int mult = -1;
            }
            while (c >= '0' && c <= '9')
            {
                iBuffer = iBuffer * 10 + c - '0';
                file++;
                c = *file;
            }

            AddNewVar(cBuffer, iBuffer * mult);
            cBuffer[0] = '\0';
            iBuffer = 0;
            break;

        case 3: // MACROS
            if (c == '@')
                file++;

            c = *file;
            int params = 0;
            while (c != ' ' && c != '\n' && c != '\0')
            {
                cBuffer[iBuffer] = c;
                iBuffer++;
                file++;
                c = *file;

                if (c == '(')
                {
                    file++;
                    c = *file;
                    params = 1;
                    break;
                }
            }
            cBuffer[iBuffer] = '\0';
            iBuffer = 0;

            if (params)
            {
                while (c != ')')
                {
                    if (c == ' ')
                    {
                        file++;
                        c = *file;
                        continue;
                    }
                    else if (c == ',')
                    {
                        file++;
                        c = *file;

                        sBuffer[params - 1][iBuffer] = '\0';
                        params++;
                        iBuffer = 0;
                        continue;
                    }

                    sBuffer[params - 1][iBuffer] = c;
                    iBuffer++;
                    file++;
                    c = *file;
                }
                sBuffer[params - 1][iBuffer] = '\0';
                file++;
                c = *file;
            }

            iBuffer = 0;
            file++;
            c = *file;
            if (c == '"')
            {
                file++;
                c = *file;
            }

            while (c != '"' && c != '\n' && c != '\0')
            {
                cBuffer2[iBuffer] = c;
                iBuffer++;
                file++;
                c = *file;
            }
            cBuffer2[iBuffer] = '\0';

            AddNewMacro(cBuffer, cBuffer2, params, sBuffer);
            cBuffer[0] = '\0';
            cBuffer2[0] = '\0';
            iBuffer = 0;
            break;

        default: // NULL
            break;
        }
    }
}

void SaveConfig()
{
}

int main(int argsc, char *argsv[])
{
    LoadConfig();

    if (GLOBAL_CONFIGS[SHOW_LOADED])
    {
        for (int i = 0; i < CONFIG_COUNT; i++)
            printf("[%s]: [%d]\n", GLOBAL_NAMES[i], GLOBAL_CONFIGS[i]);

        printf("\n");
        for (int i = 0; i < varCount; i++)
        {
            Var var = vars[i];
            printf("$[%s]: [%d]\n", var.name, var.value);
        }

        printf("\n");
        for (int i = 0; i < macroCount; i++)
        {
            Macro mac = macros[i];
            printf("@[%s(", mac.name);
            for (int i = 0; i < mac.paramCount; i++)
            {
                printf("%s", mac.parameters[i]);
                if (i + 1 < mac.paramCount)
                    printf(", ");
            }
            printf(")]: [%s]\n", mac.value);
        }
        printf("\n");
    }

    if (argsc == 1)
    {
        bool end = false;
        while (!end)
        {
            printf("Type Dice Values: ");
            char buffer[256];
            fgets(buffer, 256, stdin);
            Program(buffer);

            printf("Continue [y/n]? ");
            fgets(buffer, 256, stdin);
            if (buffer[0] == 'n')
                end = true;
        }
    }
    else
        Program(argsv[1]);

    SaveConfig();
    return 0;
}