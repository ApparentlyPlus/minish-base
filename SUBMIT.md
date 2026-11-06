# minish - Αναφορά Υλοποίησης & Design Choices

**Μάθημα:** Λειτουργικά Συστήματα (AIC303)

**Φοιτητής:** Παναγιώτης Χατζηκαλλιάς (ics24115)

## Disclaimer

Ξέρω οτί η αναφορά υποτίθεται πως είναι σύντομη, αλλά είπα να εξηγήσω λίγο ποιο αναλυτικά το τι έκανα.

## 1. Εισαγωγή & Γενική Προσέγγιση

Αρχικά, διαβάζοντας την εκφώνηση και κατεβάζοντας το starter repo (`minish-base`), αφιέρωσα λίγο χρόνο να καταλάβω πώς είναι στημένο το project. Όταν διάβαζα το PDF, χωρίς να έχω δεί τον κώδικα, ήδη υποψιαζόμουν οτί για arbitrary commands, ειδικά με redirects και pipes, δεν θα έφτανε ένα απλό regex implementation. Ήμουν έτοιμος να γράψω κανονικότατο AST parser from scratch. Για καλή μου τύχη όμως, ο parser μας δίνεται έτοιμος (`parser.c` / `parser.h`), άρα τουλάχιστον σώθηκε ένα 3ωρο από το απόγευμά μου.

Όλη η δουλειά ουσιαστικά ήταν να πάρω το `struct pipeline` που επιστρέφει η `parse_line()` και να υλοποιήσω την `execute_pipeline()` στο `src/shell.c`, μαζί με ό,τι helper functions χρειάζεται.

Η βασική ροή στη `main()` είναι straightforward:
1. Τυπώνεται το prompt (`print_prompt()`).
2. Διαβάζεται η γραμμή του χρήστη με `read_line()`.
3. Περνάει parsing με τη `parse_line()`.
4. Το resulting pipeline εκτελείται στην `execute_pipeline()`.
5. Γίνεται cleanup της μνήμης του parser με τη `pipeline_free()`.

## 2. Built-in Commands & Dispatch Logic

### Γιατί built-ins;

Κυθριώς γιατί το λέει η εκφώνηση, αλλά...

Εντολές όπως η `cd` και η `exit` δεν γίνεται να τρέξουν ως external binaries μέσω `fork()` και `execvp()` anyway. Αν κάναμε fork για την `cd`, το child process θα άλλαζε directory στο δικό του isolated περιβάλλον, και μόλις τερμάτιζε, το parent shell θα έμενε ακριβώς εκεί που ήταν. Αντίστοιχα, η `exit` πρέπει να τερματίσει το ίδιο το shell. Άρα, αυτές εκτελούνται natively κατευθείαν από το shell process.

### Clean Dispatcher

Αντί να γεμίσω την `execute_pipeline()` με `if (strcmp(...))`, έστησα έναν απλό πίνακα για dispatching:

```c
struct builtin {
    const char *name;
    size_t margs; // max arguments a command can take (args <= margs)
    int (*run)(struct command *cmd);
};

static const struct builtin b_ins[] = {
    { "cd", 1, b_cd },
    { "exit", 0, b_exit },
};
```

Με τη βοήθεια της `ffunc(cmd->argv[0])`, τσεκάρουμε αν η εντολή είναι built-in. Αν είναι, παίρνουμε τον function pointer του handler της και ελέγχουμε άμεσα το μέγιστο πλήθος παραμέτρων (`margs`) που επιτρέπεται να έχει (το οποίο είναι hardcoded για τα built-ins). Παραδείγματα όπως `exit 1 2` ή `cd a b` δεν εκτελούνται και εμφανίζουν αμέσως error message.

### Επιλογές για την `cd`

- Στην αρχή είχα φτιάξει μια βοηθητική συνάρτηση `validate_path` με έξτρα ελέγχους, αλλά γρήγορα παρατήρησα ότι έκανε consume τα native error messages του συστήματος (πχ. permission denied vs not a directory). Την έβγαλα τελείως. Η `b_cd()` καλεί κατευθείαν την `chdir()`, και αν αυτή αποτύχει, το σφάλμα κάνει propagate στην `strerror(errno)`.
- **Handling για `~` και `-`:**
  - Αν η `cd` τρέξει χωρίς args (`argc == 1`), θεωρείται αυτόματα `cd ~` (εκφώνηση)
  - Αν το path ξεκινάει με `~`, αντικαθίσταται με το `$HOME` μέσω `getenv("HOME")`
  - Αν δοθεί `-`, χρησιμοποιείται το global variable `oldcwd` για να επιστρέψει το shell στον προηγούμενο φάκελο (όπως στο κανονικό bash).
- Μετά από κάθε επιτυχημένη `chdir()`, ενημερώνεται το `oldcwd` και καλείται η `setcwd()` για να πάρει το νέο path με `getcwd()` ώστε να εμφανίζεται σωστά στο prompt.

> **Fun Note για την `clear`:** Αρχικά είχα σκεφτεί να βάλω και την `clear` στα built-ins, και να καλώ `system("clear")`. Όμως ή `system()` κάνει και αυτή fork-exec, οπότε το non-built-in flow του minish (που κάνει fork-exec) θα την κάλυπτε anyway.

## 3. Διεργασίες, Lifetimes & το δίλημμα `pids` vs `jobs`

Όταν πρωτοείδα το template, μπερδεύτηκα λίγο σχετικά με το πώς λειτουργούν τα pipelines και γιατί χρειαζόμαστε και τοπικό πίνακα `pids` και καθολικό πίνακα `jobs`. 

Αρχικά νόμιζα ότι το foreground/background flag αφορόυσε κάθε μία μεμονωμένη εντολή (όχι όλες της εντολές chained σε ένα line). Διαβάζοντας προσεκτικά το `struct pipeline` συνειδητοποίησα ότι το flag `pipeline->background` ορίζεται σε επίπεδο **ολόκληρης της γραμμής εισόδου**. Αυτό κάνει τα πράγματα πολύ πιο εύκολα. Είτε ολόκληρο το pipeline τρέχει στο foreground, είτε όλο τρέχει στο background με `&`.

### Μετά συνειδοτοποίησα γιατί χρειαζόμαστε και `pids` και `jobs`

Έχουν δύο εντελώς διαφορετικά goals:

1. **Ο πίνακας `pids`:**

   - Δεσμεύεται δυναμικά στην `execute_pipeline()` για να κρατήσει τα PIDs των παιδιών που μόλις κάναμε fork στο τρέχον pipeline.
   - Αν το pipeline είναι foreground, ο parent κάνει loop και περιμένει σειριακά όλα τα παιδιά με `waitpid(pids[i], NULL, 0)`.
   - Μόλις τελειώσουν, γίνεται `free(pids)` και επιστρέφουμε στο prompt.

2. **Ο πίνακας `jobs`:**

   - Χρησιμοποιείται αποκλειστικά για background εντολές τύπου `sleep 10 &`. Εδώ το shell ξεκινάει τα processes, τα καταγράφει στο `jobs` μέσω της `add_bg_job()` και επιστρέφει **αμέσως** στο prompt (non blocking).
   - Όταν ένα background process τελειώσει, μετατρέπεται σε zombie proc μέχρι ο γονέας να παραλάβει το exit status του.

## 4. Signal Handling & Zombies

Για τα signals έφτιαξα μια απλή helper function, την `reghandler()`, που κάνει wrap τη κλήση της `sigaction()`:

```c
static int reghandler(int signo, void (*handler)(int), int flags)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = flags;
    return sigaction(signo, &sa, NULL);
}
```

### 1. `SIGCHLD` & Αυτόματο Reaping
- Στο startup του shell δηλώνουμε handler για το `SIGCHLD` με flag `SA_RESTART`.
- Ο handler καλεί την `reap_bg_jobs()`, η οποία τσεκάρει όλα τα active slots του πίνακα `jobs` με `waitpid(jobs[i].pid, &status, WNOHANG)`. Αν κάποιο process έχει τελειώσει, απελευθερώνεται το slot και δεν μένουν zombies.

### 2. `SIGINT` (Ctrl+C)
- Στο parent shell, το `SIGINT` αγνοείται πλήρως (`SIG_IGN`). Αν ο χρήστης πατήσει Ctrl+C στο prompt, το shell παραμένει ζωντανό.
- Στα child processes:
  - Αν η εντολή είναι **foreground**, αμέσως μετά το `fork()` και πριν την `execvp()`, επαναφέρουμε το default behavior με `SIG_DFL` (άρα τα CTRL + C πιάνουν κανονικά). Έτσι, αν τρέχει μια αργή εντολή (πχ. `sleep 10`), το Ctrl+C θα την σκοτώσει και το shell θα επιστρέψει στο prompt.
  - Αν η εντολή είναι **background**, διατηρεί το `SIG_IGN` ώστε το Ctrl+C στο τερματικό να μην σκοτώνει τα background jobs.

## 5. Pipes και Redirects

### Pipe Setup & Safe Error Handling
Για ένα pipeline με $N$ εντολές, χρειαζόμαστε $N - 1$ pipes. Αντί να τα φτιάχνω ενδιάμεσα, δεσμεύω έναν top level πίνακα και ανοίγω pipes άμεσα:

```c
int pipes[pipeline->command_count][2];
size_t np;
for (np = 0; np + 1 < pipeline->command_count; np++) {
    if (pipe(pipes[np]) == -1) {
        result = -1;
        break;
    }
}
```

Κρατάω τον μετρητή `np` γιατί αν κάποιο `pipe()` αποτύχει (πχ. λόγω `EMFILE` / file descriptor limit), τότε κλείνω όσα pipes πρόλαβα να ανοίξω πριν επιστρέψω (`0` έως `np`).

### Στο Child Process:

1. **Σύνδεση pipes:**

   - Αν δεν είναι η πρώτη εντολή (`cnt > 0`), συνδέω το stdin της με το read end του προηγούμενου pipe: `dup2(pipes[cnt - 1][0], STDIN_FILENO)`.
   - Αν δεν είναι η τελευταία εντολή (`cnt + 1 < command_count`), συνδέω το stdout της με το write end του επόμενου pipe (`dup2(pipes[cnt][1], STDOUT_FILENO)`).

2. **Κλείσιμο ΟΛΩΝ των pipe fds στο child:**
   - Το child κλείνει και τα δύο άκρα όλων των pipes μετά τα σχετικά `dup2`.

### Redirections: 

Τα redirects αρχείων (`<`, `>`, `>>`) εφαρμόζονται **αμέσως μετά** τη διασύνδεση των pipes:

- `REDIRECT_INPUT` (`<`): `open(..., O_RDONLY)` και `dup2` στο `STDIN_FILENO`.
- `REDIRECT_OUTPUT` (`>`): `open(..., O_WRONLY | O_CREAT | O_TRUNC, 0644)` και `dup2` στο `STDOUT_FILENO`.
- `REDIRECT_APPEND` (`>>`): `open(..., O_WRONLY | O_CREAT | O_APPEND, 0644)` και `dup2` στο `STDOUT_FILENO`.

**Το σκεπτικό πίσω από τη σειρά:**

Βάζοντας τα redirects μετά τα pipes, αν μια εντολή βρίσκεται μέσα σε pipeline αλλά έχει και redirect εξόδου (π.χ. `cat file.txt | sort > sorted.txt`), το `dup2` του redirect αρχείου αντικαθιστά το `STDOUT_FILENO` του pipe. Έτσι, το τελικό output πάει όντως στο αρχείο και όχι στο επόμενο stage, όπως ακριβώς κάνει και το bash!

### `_exit()` vs `exit()` (this bit me hard)
Σε περίπτωση σφάλματος μέσα σε κάποιο child process (πχ. αν αποτύχει το `open()` ενός αρχείου ή η `execvp()`), καλώ την `_exit(1)` και **όχι** την `exit(1)`. 

Η απλή `exit()` τρέχει τα `atexit()` handlers του parent (όπως η επαναφορά του τερματικού) και κάνει flush τα κοινά buffers. Με την `_exit(1)` αποφεύγουμε interactions με το parent process.

## 6. Polish, Robustness & Prompt Bonus

### Prompt με Current Working Directory (Μπόνους 5%)
Υλοποιήθηκε πλήρως η εμφάνιση του φακέλου στο prompt:
```c
printf(BOLD MAGENTA "minish" RESET BOLD ":" GREEN "%s" RESET "$ ", cwd);
```
Χρησιμοποιούνται ANSI escape codes για styling, ενώ το `cwd` ενημερώνεται δυναμικά από την `setcwd()` στο startup και σε κάθε `cd`.

### Το bug με το Tab Eating (Terminal Settings)
Στις πρώτες δοκιμές στο τερματικό, παρατήρησα μια σπαστική συμπεριφορά. Aν ο χρήστης πατούσε Tab, ο χαρακτήρας tab καταλάμβανε πολλές κενές στήλες. Πατώντας Backspace, ο κέρσορας πήγαινε πίσω 5 θέσεις και έσβηνε κομμάτι από το ίδιο το prompt του shell!

Για να το λύσω, ρύθμισα το τερματικό με τη βοήθεια της `termios`:

- Στη `setup_terminal()`, θέτουμε το flag `TAB3` στα output flags, ώστε τα tabs να γίνονται κατευθείαν spaces (`t.c_oflag = (t.c_oflag & ~TABDLY) | TAB3;`).
- Μέσω της `atexit(restore_terminal)` εξασφαλίζουμε ότι όταν το minish κλείσει, το τερματικό επανέρχεται ακριβώς στις αρχικές του ρυθμίσεις.

## 7. Git Architecture: `solution` vs `finesse`

Σε κάποια φάση του project είπα να το διασκεδάσω λίγο παραπάνω και πρόσθεσα μια εντολή `about` με terminal animations (fork bomb με γάτες, nyan cat, matrix mode κλπ. σε ξεχωριστό αρχείο `art.c`), καθώς και ένα welcome banner στην εκκίνηση.

Επειδή όμως στο πανεπιστήμιο τα automated grading scripts συχνά κάνουν build με δικό τους Makefile και περιμένουν αυστηρά συγκεκριμένο output χωρίς έξτρα αρχεία ή warnings:
- **Απομόνωσα όλο το eye candy στο branch `finesse`.**
- Το branch `solution` (το οποίο παραδίδεται) διατηρήθηκε απόλυτα καθαρό, minimal και 100% compliant με την εκφώνηση και τις προδιαγραφές των τεστ, χωρίς κανένα περιττό dependency.
- Ο rendering κώδικας στο `finesse` είναι γραμμένος από το claude, ενώ όλος ο υπόλοιπος κώδικας είναι hand written by me.

## 8. Test Cases & Validation

Για να σιγουρευτώ ότι όλα παίζουν σωστά, δοκίμασα διάφορα:

| Test Case | Εντολή | Παρατήρηση |
|---|---|---|
| **Simple command** | `ls -la`, `echo hello` | Βασικό fork-exec και σωστό waiting |
| **Builtin `cd`** | `cd /tmp && pwd` | Αλλαγή directory και ανανέωση του prompt |
| **Builtin `cd ~`** | `cd` ή `cd ~` | Μετάβαση στο home directory (`$HOME`) |
| **Builtin `cd -`** | `cd -` | Επιστροφή στον προηγούμενο φάκελο (`oldcwd`) |
| **Builtin errors** | `cd /fakepath`, `exit 1 2` | Σωστά error messages (`strerror` / `margs`) χωρίς crash |
| **Background job** | `sleep 2 &` | Άμεσο return στο prompt, καταγραφή στο `jobs`, reap του zombie μετά από 2s |
| **Signal test (Ctrl+C)** | `sleep 5` + `Ctrl+C` | Τερματισμός της foreground εντολής, το shell μένει όρθιο και ξαναβγάζει prompt |
| **Ctrl+C στο prompt** | `Ctrl+C` χωρίς εντολή | Το σήμα αγνοείται πλήρως (`SIG_IGN`), δεν κλείνει το shell |
| **Redirection write** | `echo "test" > out.txt` | Δημιουργία/truncate αρχείου (0644) |
| **Redirection append**| `echo "again" >> out.txt`| Append στο τέλος του αρχείου (`O_APPEND`) |
| **Redirection read** | `cat < out.txt` | Σωστό διάβασμα από αρχείο (`O_RDONLY`) |
| **Multi-stage pipe** | `ls / \| wc -l` | Σωστό pipe 2 εντολών |
| **3-stage pipe (Bonus)**| `ls -l / \| grep usr \| wc -l` | 3 processes ταυτόχρονα, σωστό κλείσιμο όλων των pipe ends χωρίς hang |
| **Pipe + Redirect (Bonus)**| `cat < in.txt \| sort > out.txt` | Ταυτόχρονο pipe και redirected stdin/stdout |
| **Edge cases** | `Enter` (empty line), invalid binary | Κανένα crash, σωστό `perror` για ανύπαρκτη εντολή |

## 9. Build & Run

Μεταγλώττιση με τα flags της εκφώνησης (`-std=c11 -Wall -Wextra -Wpedantic -Werror`):

```bash
make clean
make
```

Εκτέλεση interactive:

```bash
./build/minish
```

Ή non-interactive (για automated testing):

```bash
printf 'echo hello\ncd /tmp\npwd\nexit\n' | ./build/minish
```
