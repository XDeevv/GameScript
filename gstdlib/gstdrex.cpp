/* see copyright notice in GameScript.h */
#include <GameScript.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <gstdstring.h>

#ifdef _DEBUG
#include <stdio.h>

static const GSChar *g_nnames[] =
{
    _SC("NONE"),_SC("OP_GREEDY"),   _SC("OP_OR"),
    _SC("OP_EXPR"),_SC("OP_NOCAPEXPR"),_SC("OP_DOT"),   _SC("OP_CLASS"),
    _SC("OP_CCLASS"),_SC("OP_NCLASS"),_SC("OP_RANGE"),_SC("OP_CHAR"),
    _SC("OP_EOL"),_SC("OP_BOL"),_SC("OP_WB"),_SC("OP_MB")
};

#endif

#define OP_GREEDY       (MAX_CHAR+1) // * + ? {n}
#define OP_OR           (MAX_CHAR+2)
#define OP_EXPR         (MAX_CHAR+3) //parentesis ()
#define OP_NOCAPEXPR    (MAX_CHAR+4) //parentesis (?:)
#define OP_DOT          (MAX_CHAR+5)
#define OP_CLASS        (MAX_CHAR+6)
#define OP_CCLASS       (MAX_CHAR+7)
#define OP_NCLASS       (MAX_CHAR+8) //negates class the [^
#define OP_RANGE        (MAX_CHAR+9)
#define OP_CHAR         (MAX_CHAR+10)
#define OP_EOL          (MAX_CHAR+11)
#define OP_BOL          (MAX_CHAR+12)
#define OP_WB           (MAX_CHAR+13)
#define OP_MB           (MAX_CHAR+14) //match balanced

#define GSREX_SYMBOL_ANY_CHAR ('.')
#define GSREX_SYMBOL_GREEDY_ONE_OR_MORE ('+')
#define GSREX_SYMBOL_GREEDY_ZERO_OR_MORE ('*')
#define GSREX_SYMBOL_GREEDY_ZERO_OR_ONE ('?')
#define GSREX_SYMBOL_BRANCH ('|')
#define GSREX_SYMBOL_END_OF_STRING ('$')
#define GSREX_SYMBOL_BEGINNING_OF_STRING ('^')
#define GSREX_SYMBOL_ESCAPE_CHAR ('\\')


typedef int GSRexNodeType;

typedef struct tagGSRexNode{
    GSRexNodeType type;
    GSInteger left;
    GSInteger right;
    GSInteger next;
}GSRexNode;

struct GSRex{
    const GSChar *_eol;
    const GSChar *_bol;
    const GSChar *_p;
    GSInteger _first;
    GSInteger _op;
    GSRexNode *_nodes;
    GSInteger _nallocated;
    GSInteger _nsize;
    GSInteger _nsubexpr;
    GSRexMatch *_matches;
    GSInteger _currsubexp;
    void *_jmpbuf;
    const GSChar **_error;
};

static GSInteger gstd_rex_list(GSRex *exp);

static GSInteger gstd_rex_newnode(GSRex *exp, GSRexNodeType type)
{
    GSRexNode n;
    n.type = type;
    n.next = n.right = n.left = -1;
    if(type == OP_EXPR)
        n.right = exp->_nsubexpr++;
    if(exp->_nallocated < (exp->_nsize + 1)) {
        GSInteger oldsize = exp->_nallocated;
        exp->_nallocated *= 2;
        exp->_nodes = (GSRexNode *)GS_realloc(exp->_nodes, oldsize * sizeof(GSRexNode) ,exp->_nallocated * sizeof(GSRexNode));
    }
    exp->_nodes[exp->_nsize++] = n;
    GSInteger newid = exp->_nsize - 1;
    return (GSInteger)newid;
}

static void gstd_rex_error(GSRex *exp,const GSChar *error)
{
    if(exp->_error) *exp->_error = error;
    longjmp(*((jmp_buf*)exp->_jmpbuf),-1);
}

static void gstd_rex_expect(GSRex *exp, GSInteger n){
    if((*exp->_p) != n)
        gstd_rex_error(exp, _SC("expected paren"));
    exp->_p++;
}

static GSChar gstd_rex_escapechar(GSRex *exp)
{
    if(*exp->_p == GSREX_SYMBOL_ESCAPE_CHAR){
        exp->_p++;
        switch(*exp->_p) {
        case 'v': exp->_p++; return '\v';
        case 'n': exp->_p++; return '\n';
        case 't': exp->_p++; return '\t';
        case 'r': exp->_p++; return '\r';
        case 'f': exp->_p++; return '\f';
        default: return (*exp->_p++);
        }
    } else if(!scisprint(*exp->_p)) gstd_rex_error(exp,_SC("letter expected"));
    return (*exp->_p++);
}

static GSInteger gstd_rex_charclass(GSRex *exp,GSInteger classid)
{
    GSInteger n = gstd_rex_newnode(exp,OP_CCLASS);
    exp->_nodes[n].left = classid;
    return n;
}

static GSInteger gstd_rex_charnode(GSRex *exp,GSBool isclass)
{
    GSChar t;
    if(*exp->_p == GSREX_SYMBOL_ESCAPE_CHAR) {
        exp->_p++;
        switch(*exp->_p) {
            case 'n': exp->_p++; return gstd_rex_newnode(exp,'\n');
            case 't': exp->_p++; return gstd_rex_newnode(exp,'\t');
            case 'r': exp->_p++; return gstd_rex_newnode(exp,'\r');
            case 'f': exp->_p++; return gstd_rex_newnode(exp,'\f');
            case 'v': exp->_p++; return gstd_rex_newnode(exp,'\v');
            case 'a': case 'A': case 'w': case 'W': case 's': case 'S':
            case 'd': case 'D': case 'x': case 'X': case 'c': case 'C':
            case 'p': case 'P': case 'l': case 'u':
                {
                t = *exp->_p; exp->_p++;
                return gstd_rex_charclass(exp,t);
                }
            case 'm':
                {
                     GSChar cb, ce; //cb = character begin match ce = character end match
                     cb = *++exp->_p; //skip 'm'
                     ce = *++exp->_p;
                     exp->_p++; //points to the next char to be parsed
                     if ((!cb) || (!ce)) gstd_rex_error(exp,_SC("balanced chars expected"));
                     if ( cb == ce ) gstd_rex_error(exp,_SC("open/close char can't be the same"));
                     GSInteger node =  gstd_rex_newnode(exp,OP_MB);
                     exp->_nodes[node].left = cb;
                     exp->_nodes[node].right = ce;
                     return node;
                }
            case 0:
                gstd_rex_error(exp,_SC("letter expected for argument of escape sequence"));
                break;
            case 'b':
            case 'B':
                if(!isclass) {
                    GSInteger node = gstd_rex_newnode(exp,OP_WB);
                    exp->_nodes[node].left = *exp->_p;
                    exp->_p++;
                    return node;
                } //else default
            default:
                t = *exp->_p; exp->_p++;
                return gstd_rex_newnode(exp,t);
        }
    }
    else if(!scisprint(*exp->_p)) {

        gstd_rex_error(exp,_SC("letter expected"));
    }
    t = *exp->_p; exp->_p++;
    return gstd_rex_newnode(exp,t);
}
static GSInteger gstd_rex_class(GSRex *exp)
{
    GSInteger ret = -1;
    GSInteger first = -1,chain;
    if(*exp->_p == GSREX_SYMBOL_BEGINNING_OF_STRING){
        ret = gstd_rex_newnode(exp,OP_NCLASS);
        exp->_p++;
    }else ret = gstd_rex_newnode(exp,OP_CLASS);

    if(*exp->_p == ']') gstd_rex_error(exp,_SC("empty class"));
    chain = ret;
    while(*exp->_p != ']' && exp->_p != exp->_eol) {
        if(*exp->_p == '-' && first != -1){
            GSInteger r;
            if(*exp->_p++ == ']') gstd_rex_error(exp,_SC("unfinished range"));
            r = gstd_rex_newnode(exp,OP_RANGE);
            if(exp->_nodes[first].type>*exp->_p) gstd_rex_error(exp,_SC("invalid range"));
            if(exp->_nodes[first].type == OP_CCLASS) gstd_rex_error(exp,_SC("cannot use character classes in ranges"));
            exp->_nodes[r].left = exp->_nodes[first].type;
            GSInteger t = gstd_rex_escapechar(exp);
            exp->_nodes[r].right = t;
            exp->_nodes[chain].next = r;
            chain = r;
            first = -1;
        }
        else{
            if(first!=-1){
                GSInteger c = first;
                exp->_nodes[chain].next = c;
                chain = c;
                first = gstd_rex_charnode(exp,GSTrue);
            }
            else{
                first = gstd_rex_charnode(exp,GSTrue);
            }
        }
    }
    if(first!=-1){
        GSInteger c = first;
        exp->_nodes[chain].next = c;
    }
    /* hack? */
    exp->_nodes[ret].left = exp->_nodes[ret].next;
    exp->_nodes[ret].next = -1;
    return ret;
}

static GSInteger gstd_rex_parsenumber(GSRex *exp)
{
    GSInteger ret = *exp->_p-'0';
    GSInteger positions = 10;
    exp->_p++;
    while(isdigit(*exp->_p)) {
        ret = ret*10+(*exp->_p++-'0');
        if(positions==1000000000) gstd_rex_error(exp,_SC("overflow in numeric constant"));
        positions *= 10;
    };
    return ret;
}

static GSInteger gstd_rex_element(GSRex *exp)
{
    GSInteger ret = -1;
    switch(*exp->_p)
    {
    case '(': {
        GSInteger expr;
        exp->_p++;


        if(*exp->_p =='?') {
            exp->_p++;
            gstd_rex_expect(exp,':');
            expr = gstd_rex_newnode(exp,OP_NOCAPEXPR);
        }
        else
            expr = gstd_rex_newnode(exp,OP_EXPR);
        GSInteger newn = gstd_rex_list(exp);
        exp->_nodes[expr].left = newn;
        ret = expr;
        gstd_rex_expect(exp,')');
              }
              break;
    case '[':
        exp->_p++;
        ret = gstd_rex_class(exp);
        gstd_rex_expect(exp,']');
        break;
    case GSREX_SYMBOL_END_OF_STRING: exp->_p++; ret = gstd_rex_newnode(exp,OP_EOL);break;
    case GSREX_SYMBOL_ANY_CHAR: exp->_p++; ret = gstd_rex_newnode(exp,OP_DOT);break;
    default:
        ret = gstd_rex_charnode(exp,GSFalse);
        break;
    }


    GSBool isgreedy = GSFalse;
    unsigned short p0 = 0, p1 = 0;
    switch(*exp->_p){
        case GSREX_SYMBOL_GREEDY_ZERO_OR_MORE: p0 = 0; p1 = 0xFFFF; exp->_p++; isgreedy = GSTrue; break;
        case GSREX_SYMBOL_GREEDY_ONE_OR_MORE: p0 = 1; p1 = 0xFFFF; exp->_p++; isgreedy = GSTrue; break;
        case GSREX_SYMBOL_GREEDY_ZERO_OR_ONE: p0 = 0; p1 = 1; exp->_p++; isgreedy = GSTrue; break;
        case '{':
            exp->_p++;
            if(!isdigit(*exp->_p)) gstd_rex_error(exp,_SC("number expected"));
            p0 = (unsigned short)gstd_rex_parsenumber(exp);
            /*******************************/
            switch(*exp->_p) {
        case '}':
            p1 = p0; exp->_p++;
            break;
        case ',':
            exp->_p++;
            p1 = 0xFFFF;
            if(isdigit(*exp->_p)){
                p1 = (unsigned short)gstd_rex_parsenumber(exp);
            }
            gstd_rex_expect(exp,'}');
            break;
        default:
            gstd_rex_error(exp,_SC(", or } expected"));
            }
            /*******************************/
            isgreedy = GSTrue;
            break;

    }
    if(isgreedy) {
        GSInteger nnode = gstd_rex_newnode(exp,OP_GREEDY);
        exp->_nodes[nnode].left = ret;
        exp->_nodes[nnode].right = ((p0)<<16)|p1;
        ret = nnode;
    }

    if((*exp->_p != GSREX_SYMBOL_BRANCH) && (*exp->_p != ')') && (*exp->_p != GSREX_SYMBOL_GREEDY_ZERO_OR_MORE) && (*exp->_p != GSREX_SYMBOL_GREEDY_ONE_OR_MORE) && (*exp->_p != '\0')) {
        GSInteger nnode = gstd_rex_element(exp);
        exp->_nodes[ret].next = nnode;
    }

    return ret;
}

static GSInteger gstd_rex_list(GSRex *exp)
{
    GSInteger ret=-1,e;
    if(*exp->_p == GSREX_SYMBOL_BEGINNING_OF_STRING) {
        exp->_p++;
        ret = gstd_rex_newnode(exp,OP_BOL);
    }
    e = gstd_rex_element(exp);
    if(ret != -1) {
        exp->_nodes[ret].next = e;
    }
    else ret = e;

    if(*exp->_p == GSREX_SYMBOL_BRANCH) {
        GSInteger temp,tright;
        exp->_p++;
        temp = gstd_rex_newnode(exp,OP_OR);
        exp->_nodes[temp].left = ret;
        tright = gstd_rex_list(exp);
        exp->_nodes[temp].right = tright;
        ret = temp;
    }
    return ret;
}

static GSBool gstd_rex_matchcclass(GSInteger cclass,GSChar c)
{
    switch(cclass) {
    case 'a': return isalpha(c)?GSTrue:GSFalse;
    case 'A': return !isalpha(c)?GSTrue:GSFalse;
    case 'w': return (isalnum(c) || c == '_')?GSTrue:GSFalse;
    case 'W': return (!isalnum(c) && c != '_')?GSTrue:GSFalse;
    case 's': return isspace(c)?GSTrue:GSFalse;
    case 'S': return !isspace(c)?GSTrue:GSFalse;
    case 'd': return isdigit(c)?GSTrue:GSFalse;
    case 'D': return !isdigit(c)?GSTrue:GSFalse;
    case 'x': return isxdigit(c)?GSTrue:GSFalse;
    case 'X': return !isxdigit(c)?GSTrue:GSFalse;
    case 'c': return iscntrl(c)?GSTrue:GSFalse;
    case 'C': return !iscntrl(c)?GSTrue:GSFalse;
    case 'p': return ispunct(c)?GSTrue:GSFalse;
    case 'P': return !ispunct(c)?GSTrue:GSFalse;
    case 'l': return islower(c)?GSTrue:GSFalse;
    case 'u': return isupper(c)?GSTrue:GSFalse;
    }
    return GSFalse; /*cannot happen*/
}

static GSBool gstd_rex_matchclass(GSRex* exp,GSRexNode *node,GSChar c)
{
    do {
        switch(node->type) {
            case OP_RANGE:
                if(c >= node->left && c <= node->right) return GSTrue;
                break;
            case OP_CCLASS:
                if(gstd_rex_matchcclass(node->left,c)) return GSTrue;
                break;
            default:
                if(c == node->type)return GSTrue;
        }
    } while((node->next != -1) && (node = &exp->_nodes[node->next]));
    return GSFalse;
}

static const GSChar *gstd_rex_matchnode(GSRex* exp,GSRexNode *node,const GSChar *str,GSRexNode *next)
{

    GSRexNodeType type = node->type;
    switch(type) {
    case OP_GREEDY: {
        //GSRexNode *greedystop = (node->next != -1) ? &exp->_nodes[node->next] : NULL;
        GSRexNode *greedystop = NULL;
        GSInteger p0 = (node->right >> 16)&0x0000FFFF, p1 = node->right&0x0000FFFF, nmaches = 0;
        const GSChar *s=str, *good = str;

        if(node->next != -1) {
            greedystop = &exp->_nodes[node->next];
        }
        else {
            greedystop = next;
        }

        while((nmaches == 0xFFFF || nmaches < p1)) {

            const GSChar *stop;
            if(!(s = gstd_rex_matchnode(exp,&exp->_nodes[node->left],s,greedystop)))
                break;
            nmaches++;
            good=s;
            if(greedystop) {
                //checks that 0 matches satisfy the expression(if so skips)
                //if not would always stop(for instance if is a '?')
                if(greedystop->type != OP_GREEDY ||
                (greedystop->type == OP_GREEDY && ((greedystop->right >> 16)&0x0000FFFF) != 0))
                {
                    GSRexNode *gnext = NULL;
                    if(greedystop->next != -1) {
                        gnext = &exp->_nodes[greedystop->next];
                    }else if(next && next->next != -1){
                        gnext = &exp->_nodes[next->next];
                    }
                    stop = gstd_rex_matchnode(exp,greedystop,s,gnext);
                    if(stop) {
                        //if satisfied stop it
                        if(p0 == p1 && p0 == nmaches) break;
                        else if(nmaches >= p0 && p1 == 0xFFFF) break;
                        else if(nmaches >= p0 && nmaches <= p1) break;
                    }
                }
            }

            if(s >= exp->_eol)
                break;
        }
        if(p0 == p1 && p0 == nmaches) return good;
        else if(nmaches >= p0 && p1 == 0xFFFF) return good;
        else if(nmaches >= p0 && nmaches <= p1) return good;
        return NULL;
    }
    case OP_OR: {
            const GSChar *asd = str;
            GSRexNode *temp=&exp->_nodes[node->left];
            while( (asd = gstd_rex_matchnode(exp,temp,asd,NULL)) ) {
                if(temp->next != -1)
                    temp = &exp->_nodes[temp->next];
                else
                    return asd;
            }
            asd = str;
            temp = &exp->_nodes[node->right];
            while( (asd = gstd_rex_matchnode(exp,temp,asd,NULL)) ) {
                if(temp->next != -1)
                    temp = &exp->_nodes[temp->next];
                else
                    return asd;
            }
            return NULL;
            break;
    }
    case OP_EXPR:
    case OP_NOCAPEXPR:{
            GSRexNode *n = &exp->_nodes[node->left];
            const GSChar *cur = str;
            GSInteger capture = -1;
            if(node->type != OP_NOCAPEXPR && node->right == exp->_currsubexp) {
                capture = exp->_currsubexp;
                exp->_matches[capture].begin = cur;
                exp->_currsubexp++;
            }
            GSInteger tempcap = exp->_currsubexp;
            do {
                GSRexNode *subnext = NULL;
                if(n->next != -1) {
                    subnext = &exp->_nodes[n->next];
                }else {
                    subnext = next;
                }
                if(!(cur = gstd_rex_matchnode(exp,n,cur,subnext))) {
                    if(capture != -1){
                        exp->_matches[capture].begin = 0;
                        exp->_matches[capture].len = 0;
                    }
                    return NULL;
                }
            } while((n->next != -1) && (n = &exp->_nodes[n->next]));

            exp->_currsubexp = tempcap;
            if(capture != -1)
                exp->_matches[capture].len = cur - exp->_matches[capture].begin;
            return cur;
    }
    case OP_WB:
        if((str == exp->_bol && !isspace(*str))
         || (str == exp->_eol && !isspace(*(str-1)))
         || (!isspace(*str) && isspace(*(str+1)))
         || (isspace(*str) && !isspace(*(str+1))) ) {
            return (node->left == 'b')?str:NULL;
        }
        return (node->left == 'b')?NULL:str;
    case OP_BOL:
        if(str == exp->_bol) return str;
        return NULL;
    case OP_EOL:
        if(str == exp->_eol) return str;
        return NULL;
    case OP_DOT:{
        if (str == exp->_eol) return NULL;
        str++;
                }
        return str;
    case OP_NCLASS:
    case OP_CLASS:
        if (str == exp->_eol) return NULL;
        if(gstd_rex_matchclass(exp,&exp->_nodes[node->left],*str)?(type == OP_CLASS?GSTrue:GSFalse):(type == OP_NCLASS?GSTrue:GSFalse)) {
            str++;
            return str;
        }
        return NULL;
    case OP_CCLASS:
        if (str == exp->_eol) return NULL;
        if(gstd_rex_matchcclass(node->left,*str)) {
            str++;
            return str;
        }
        return NULL;
    case OP_MB:
        {
            GSInteger cb = node->left; //char that opens a balanced expression
            if(*str != cb) return NULL; // string doesnt start with open char
            GSInteger ce = node->right; //char that closes a balanced expression
            GSInteger cont = 1;
            const GSChar *streol = exp->_eol;
            while (++str < streol) {
              if (*str == ce) {
                if (--cont == 0) {
                    return ++str;
                }
              }
              else if (*str == cb) cont++;
            }
        }
        return NULL; // string ends out of balance
    default: /* char */
        if (str == exp->_eol) return NULL;
        if(*str != node->type) return NULL;
        str++;
        return str;
    }
    return NULL;
}

/* public api */
GSRex *gstd_rex_compile(const GSChar *pattern,const GSChar **error)
{
    GSRex * volatile exp = (GSRex *)GS_malloc(sizeof(GSRex)); // "volatile" is needed for setjmp()
    exp->_eol = exp->_bol = NULL;
    exp->_p = pattern;
    exp->_nallocated = (GSInteger)scstrlen(pattern) * sizeof(GSChar);
    exp->_nodes = (GSRexNode *)GS_malloc(exp->_nallocated * sizeof(GSRexNode));
    exp->_nsize = 0;
    exp->_matches = 0;
    exp->_nsubexpr = 0;
    exp->_first = gstd_rex_newnode(exp,OP_EXPR);
    exp->_error = error;
    exp->_jmpbuf = GS_malloc(sizeof(jmp_buf));
    if(setjmp(*((jmp_buf*)exp->_jmpbuf)) == 0) {
        GSInteger res = gstd_rex_list(exp);
        exp->_nodes[exp->_first].left = res;
        if(*exp->_p!='\0')
            gstd_rex_error(exp,_SC("unexpected character"));
#ifdef _DEBUG
        {
            GSInteger nsize,i;
            GSRexNode *t;
            nsize = exp->_nsize;
            t = &exp->_nodes[0];
            scprintf(_SC("\n"));
            for(i = 0;i < nsize; i++) {
                if(exp->_nodes[i].type>MAX_CHAR)
                    scprintf(_SC("[%02d] %10s "), (GSInt32)i,g_nnames[exp->_nodes[i].type-MAX_CHAR]);
                else
                    scprintf(_SC("[%02d] %10c "), (GSInt32)i,exp->_nodes[i].type);
                scprintf(_SC("left %02d right %02d next %02d\n"), (GSInt32)exp->_nodes[i].left, (GSInt32)exp->_nodes[i].right, (GSInt32)exp->_nodes[i].next);
            }
            scprintf(_SC("\n"));
        }
#endif
        exp->_matches = (GSRexMatch *) GS_malloc(exp->_nsubexpr * sizeof(GSRexMatch));
        memset(exp->_matches,0,exp->_nsubexpr * sizeof(GSRexMatch));
    }
    else{
        gstd_rex_free(exp);
        return NULL;
    }
    return exp;
}

void gstd_rex_free(GSRex *exp)
{
    if(exp) {
        if(exp->_nodes) GS_free(exp->_nodes,exp->_nallocated * sizeof(GSRexNode));
        if(exp->_jmpbuf) GS_free(exp->_jmpbuf,sizeof(jmp_buf));
        if(exp->_matches) GS_free(exp->_matches,exp->_nsubexpr * sizeof(GSRexMatch));
        GS_free(exp,sizeof(GSRex));
    }
}

GSBool gstd_rex_match(GSRex* exp,const GSChar* text)
{
    const GSChar* res = NULL;
    exp->_bol = text;
    exp->_eol = text + scstrlen(text);
    exp->_currsubexp = 0;
    res = gstd_rex_matchnode(exp,exp->_nodes,text,NULL);
    if(res == NULL || res != exp->_eol)
        return GSFalse;
    return GSTrue;
}

GSBool gstd_rex_searchrange(GSRex* exp,const GSChar* text_begin,const GSChar* text_end,const GSChar** out_begin, const GSChar** out_end)
{
    const GSChar *cur = NULL;
    GSInteger node = exp->_first;
    if(text_begin >= text_end) return GSFalse;
    exp->_bol = text_begin;
    exp->_eol = text_end;
    do {
        cur = text_begin;
        while(node != -1) {
            exp->_currsubexp = 0;
            cur = gstd_rex_matchnode(exp,&exp->_nodes[node],cur,NULL);
            if(!cur)
                break;
            node = exp->_nodes[node].next;
        }
        text_begin++;
    } while(cur == NULL && text_begin != text_end);

    if(cur == NULL)
        return GSFalse;

    --text_begin;

    if(out_begin) *out_begin = text_begin;
    if(out_end) *out_end = cur;
    return GSTrue;
}

GSBool gstd_rex_search(GSRex* exp,const GSChar* text, const GSChar** out_begin, const GSChar** out_end)
{
    return gstd_rex_searchrange(exp,text,text + scstrlen(text),out_begin,out_end);
}

GSInteger gstd_rex_getsubexpcount(GSRex* exp)
{
    return exp->_nsubexpr;
}

GSBool gstd_rex_getsubexp(GSRex* exp, GSInteger n, GSRexMatch *subexp)
{
    if( n<0 || n >= exp->_nsubexpr) return GSFalse;
    *subexp = exp->_matches[n];
    return GSTrue;
}


