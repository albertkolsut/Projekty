from django.shortcuts import render, get_object_or_404
from .models import Tour, Post
from django.shortcuts import render, redirect
from django.contrib.auth import authenticate, login, logout
from django.contrib.auth.decorators import login_required
from django.contrib.auth.mixins import LoginRequiredMixin
from django.views import View
from django.contrib.auth.models import User
from .forms import RegisterForm, PostForm
from django.utils import timezone
from .forms import CommentForm
from .models import ZawodnikStatystyka
from django.utils import timezone
from django.contrib import messages
from .models import Match, Ticket, TicketType

def process_ticket_purchase(request, match_id, quantity_to_buy_str, ticket_type_id_str):
    try:
        quantity_to_buy = int(quantity_to_buy_str)
        if quantity_to_buy < 1:
            messages.error(request, "Ilość biletów musi być większa niż 0.")
            return False, None

        if not ticket_type_id_str:
            messages.error(request, "Nie wybrano typu biletu.")
            return False, None
        ticket_type_id = int(ticket_type_id_str)

    except (ValueError, TypeError):
        messages.error(request, "Nieprawidłowa ilość lub typ biletu.")
        return False, None

    match_to_buy = get_object_or_404(Match, id=match_id)
    selected_ticket_type = get_object_or_404(TicketType, id=ticket_type_id)

    price_per_ticket_after_discount = selected_ticket_type.calculate_price(match_to_buy.base_ticket_price)

    ticket_entry, created = Ticket.objects.get_or_create(
        user=request.user,
        match=match_to_buy,
        ticket_type=selected_ticket_type,
        defaults={
            'quantity': 0,
            'price_paid_per_ticket': price_per_ticket_after_discount
        }
    )

    ticket_entry.quantity += quantity_to_buy
    ticket_entry.save()

    messages.success(request,
                     f"Pomyślnie dodano {quantity_to_buy} bilet(y) ({selected_ticket_type.name}) na mecz z {match_to_buy.opponent_name} (łączna cena: {ticket_entry.total_price_paid} PLN).")
    return True, ticket_entry


def statystyki_zawodnikow(request):
    zawodnicy = ZawodnikStatystyka.objects.all().order_by('numer')
    return render(request, 'tours/statystyki.html', {'zawodnicy': zawodnicy})
def tabela(request):
    return render(request, 'tabela.html')  

def index(request):
    tours = Tour.objects.all()
    context = {'tours':tours}
    return render(request, 'tours/index.html', context)

def register_view(request):
    if request.method == "POST":
        form = RegisterForm(request.POST)
        if form.is_valid():
            username = form.cleaned_data.get("username")
            password = form.cleaned_data.get("password")
            user = User.objects.create_user(username=username, password=password)
            login(request, user)
            return redirect('login')
    else:
        form = RegisterForm()
    return render(request, 'tours/accounts/register.html', {'form':form})


def login_view(request):
    error_message = None 
    if request.method == "POST":  
        username = request.POST.get("username")  
        password = request.POST.get("password")  
        user = authenticate(request, username=username, password=password)  
        if user is not None:  
            login(request, user)  
            next_url = request.POST.get('next') or request.GET.get('next') or 'protected'  
            return redirect(next_url) 
        else:
            error_message = "Invalid credentials"  
    return render(request, 'tours/accounts/login.html', {'error': error_message})

def logout_view(request):
    if request.user.is_authenticated:
        logout(request)
    return redirect('login')

@login_required
def home_view(request):
    return render(request, 'auth1_app/home.html')

class ProtectedView(LoginRequiredMixin, View):
    login_url = '/login/'
    redirect_field_name = 'redirect_to'
    
    def get(self, request):
        return render(request, 'registration/protected.html')


class ProtectedViewBilety(LoginRequiredMixin, View):
    login_url = 'login'
    template_name = 'tours/registration/bilety_u.html'
    base_template_name = 'tours/registration/base_u.html'

    def get(self, request, *args, **kwargs):
        context = {}
        user_is_admin = request.user.is_staff or request.user.is_superuser

        if user_is_admin:
            users_with_tickets = User.objects.filter(tickets__isnull=False).distinct().prefetch_related(
                'tickets',
                'tickets__match',
                'tickets__ticket_type'
            )
            context['users_with_tickets'] = users_with_tickets
            context['admin_view'] = True
        else:
            user_tickets = Ticket.objects.filter(user=request.user, quantity__gt=0).order_by('match__match_date',
                                                                                             'ticket_type__name').select_related(
                'match', 'ticket_type')
            context['user_tickets'] = user_tickets
            context['admin_view'] = False

        context['base_template'] = self.base_template_name
        return render(request, self.template_name, context)


class ProtectedViewAktualnosci(LoginRequiredMixin, View):
    login_url = '/login/'
    redirect_field_name = 'redirect_to'

    def get(self, request):
        posts = Post.objects.filter(published_date__lte=timezone.now()).order_by('-published_date')
        return render(request, 'registration/aktualnosci_u.html', {'posts': posts})


class ProtectedViewSklep(LoginRequiredMixin, View):
    login_url = 'login'
    template_name = 'tours/registration/sklep_u.html'
    base_template_name = 'tours/registration/base_u.html'

    def get_context_data(self, **kwargs):
        context = kwargs
        context['upcoming_matches'] = Match.objects.filter(match_date__gte=timezone.now()).order_by('match_date')
        context['ticket_types'] = TicketType.objects.all().order_by('name')
        context['base_template'] = self.base_template_name
        return context

    def get(self, request, *args, **kwargs):
        return render(request, self.template_name, self.get_context_data())

    def post(self, request, *args, **kwargs):
        match_id = request.POST.get('match_id')
        quantity_str = request.POST.get('quantity', '1')
        ticket_type_id_str = request.POST.get('ticket_type')

        process_ticket_purchase(request, match_id, quantity_str, ticket_type_id_str)
        return render(request, self.template_name, self.get_context_data())



def post_list(request):
    posts = Post.objects.filter(published_date__lte=timezone.now()).order_by('-published_date')
    return render(request, 'blog/post_list.html', {'posts': posts})

def post_detail(request, slug):
    post = get_object_or_404(Post, slug=slug)

    if request.method == "POST":
        form = CommentForm(request.POST)
        if form.is_valid():
            comment = form.save(commit=False)
            comment.post = post
            comment.author = request.user
            comment.save()
            return redirect('post_detail', slug=post.slug)
    else:
        form = CommentForm()

    comments = post.comments.filter(approved=True)
    
    base_template = "registration/base_u.html" if request.user.is_authenticated else "base.html"

    return render(request, 'post_detail.html', {
        'post': post,
        'form': form,
        'comments': comments,
        'base_template': base_template
    })

def aktualnosci(request):
    posts = Post.objects.filter(published_date__lte=timezone.now()).order_by('-published_date')
    return render(request, 'aktualnosci.html', {'posts': posts})


def aktualnosci_u(request):
    posts = Post.objects.filter(published_date__lte=timezone.now()).order_by('-published_date')
    return render(request, 'registration/aktualnosci_u.html', {'posts': posts})

class ProtectedViewStatystyki(LoginRequiredMixin, View):
    login_url = '/login/'
    redirect_field_name = 'redirect_to'

def get(self, request):
    zawodnicy = ZawodnikStatystyka.objects.all().order_by('numer')
    return render(request, 'registration/statystyki_u.html', {'zawodnicy': zawodnicy})

def add_post(request):
    if request.method == "POST":
        form = PostForm(request.POST, request.FILES)
        if form.is_valid():
            post = form.save(commit=False)
            post.author = request.user
            post.publish()
            return redirect('post_detail', slug=post.slug)
    else:
        form = PostForm()
    return render(request, 'blog/add_post.html', {'form': form})


def terminarz(request):
    all_matches = Match.objects.filter(match_date__gte=timezone.now()).order_by('match_date')
    if request.user.is_authenticated:
        base_template_path = 'tours/registration/base_u.html'
        terminarz_template_path = 'tours/registration/terminarz_u.html'
    else:
        base_template_path = 'tours/base.html'
        terminarz_template_path = 'tours/terminarz.html'

    context = {
        'matches': all_matches,
        'base_template': base_template_path,
    }
    return render(request, terminarz_template_path, context)

def tabela(request):
    if request.user.is_authenticated:
        return render(request, 'registration/tabela_u.html')
    return render(request, 'tabela.html')

def statystyki_zawodnikow(request):
    zawodnicy = ZawodnikStatystyka.objects.all().order_by('numer')
    template = 'registration/statystyki_u.html' if request.user.is_authenticated else 'tours/statystyki.html'
    return render(request, template, {'zawodnicy': zawodnicy})


@login_required(login_url='login')
def terminarz_u(request):
    all_matches = Match.objects.filter(match_date__gte=timezone.now()).order_by('match_date')
    base_template_path_for_u_pages = 'tours/registration/base_u.html'

    context = {
        'matches': all_matches,
        'base_template': base_template_path_for_u_pages,
    }
    return render(request, 'tours/registration/terminarz_u.html', context)

@login_required(login_url='/login/')
def tabela_u(request):
    return render(request, 'registration/tabela_u.html')

@login_required(login_url='/login/')
def statystyki_zawodnikow_u(request):
    zawodnicy = ZawodnikStatystyka.objects.all().order_by('numer')
    return render(request, 'registration/statystyki_u.html', {'zawodnicy': zawodnicy})


def sklep(request):
    upcoming_matches = Match.objects.filter(match_date__gte=timezone.now()).order_by('match_date')
    ticket_types = TicketType.objects.all().order_by('name')
    base_template_path = 'tours/registration/base_u.html' if request.user.is_authenticated else 'tours/base.html'

    if request.method == 'POST':
        if not request.user.is_authenticated:
            messages.info(request, "Musisz się zalogować, aby kupić bilety.")
            login_url_with_next = f"{reverse('login')}?next={reverse('sklep')}"
            return redirect(login_url_with_next)

        match_id = request.POST.get('match_id')
        quantity_str = request.POST.get('quantity', '1')
        ticket_type_id_str = request.POST.get('ticket_type')

        process_ticket_purchase(request, match_id, quantity_str, ticket_type_id_str)

        context = {
            'upcoming_matches': Match.objects.filter(match_date__gte=timezone.now()).order_by('match_date')[:3],
            'ticket_types': TicketType.objects.all().order_by('name'),
            'base_template': base_template_path
        }
        return render(request, 'tours/sklep.html', context)  

    context = {
        'upcoming_matches': upcoming_matches,
        'ticket_types': ticket_types,
        'base_template': base_template_path
    }
    return render(request, 'tours/sklep.html', context)

def bilety(request):
    if request.user.is_authenticated:
        return redirect('bilety_u')
    context = {
        'base_template': "tours/base.html"
    }
    return render(request, 'tours/bilety.html', context)


@login_required(login_url='login')
def return_ticket_view(request, ticket_id):
    ticket_to_return = get_object_or_404(Ticket, id=ticket_id, user=request.user)
    if request.method == 'POST':
        try:
            quantity_to_return = int(request.POST.get('quantity_to_return', 1))
            if quantity_to_return < 1 or quantity_to_return > ticket_to_return.quantity:
                messages.error(request, "Nieprawidłowa ilość biletów do zwrotu.")
                return redirect('bilety_u')
        except (ValueError, TypeError):
            messages.error(request, "Wprowadzono nieprawidłową wartość dla ilości biletów.")
            return redirect('bilety_u')

        ticket_to_return.quantity -= quantity_to_return

        if ticket_to_return.quantity <= 0:
            ticket_to_return.delete()
            messages.success(request,
                             f"Pomyślnie zwrócono wszystkie bilety ({ticket_to_return.ticket_type.name}) na mecz z {ticket_to_return.match.opponent_name}.")
        else:
            ticket_to_return.save()
            messages.success(request,
                             f"Pomyślnie zwrócono {quantity_to_return} bilet(y) ({ticket_to_return.ticket_type.name}) na mecz z {ticket_to_return.match.opponent_name}.")
        return redirect('bilety_u')
    else:
        messages.info(request, "Aby zwrócić bilet, użyj formularza na stronie 'Moje Bilety'.")
        return redirect('bilety_u')


def merch_placeholder_view(request):
    base_template_path = 'tours/registration/base_u.html' if request.user.is_authenticated else 'tours/base.html'
    if request.method == 'POST':
        if not request.user.is_authenticated:
            messages.info(request, "Musisz się zalogować, aby 'kupić' gadżet.")
            login_url_with_next = f"{reverse('login')}?next={reverse('merch')}"
            return redirect(login_url_with_next)

        item_name = request.POST.get('item_name', 'Wybrany gadżet')
        quantity_bought = request.POST.get('quantity', '1')

        messages.success(request, f"Dziękujemy za 'zakup' {quantity_bought} szt. produktu: {item_name}!")
        context = {
            'base_template': base_template_path,
        }
        return render(request, 'tours/merch.html', context)

    context = {
        'base_template': base_template_path,
    }
    return render(request, 'tours/merch.html', context)