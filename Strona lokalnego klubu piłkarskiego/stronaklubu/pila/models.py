from django.db import models
from django.contrib.auth.models import User
from django.utils import timezone
from django.conf import settings
from decimal import Decimal
# Create your models here.

class ZawodnikStatystyka(models.Model):
    imie_nazwisko = models.CharField(max_length=100)
    numer = models.PositiveIntegerField()
    wystepy = models.PositiveIntegerField()
    gole = models.PositiveIntegerField()
    asysty = models.PositiveIntegerField()
    zolt_kartki = models.PositiveIntegerField()
    czer_kartki = models.PositiveIntegerField()

    def __str__(self):
        return f"{self.imie_nazwisko} ({self.numer})"

class Tour(models.Model):
    origin_country = models.CharField(max_length=64)
    destynation_country = models.CharField(max_length=64)
    number_of_nights = models.IntegerField()
    price = models.IntegerField()

    def __str__(self):
        return(f"ID:{self.id}: {self.origin_country}")

class Post(models.Model):
    title = models.CharField(max_length=200)
    content = models.TextField()
    image = models.ImageField(upload_to='post_images/', blank=True, null=True)
    author = models.ForeignKey(User, on_delete=models.CASCADE)
    created_date = models.DateTimeField(default=timezone.now)
    published_date = models.DateTimeField(blank=True, null=True)
    slug = models.SlugField(max_length=200, unique=True)

    def publish(self):
        self.published_date = timezone.now()
        self.save()
        
    def __str__(self):
        return self.title


class Comment(models.Model):
    post = models.ForeignKey(Post, on_delete=models.CASCADE, related_name='comments')
    author = models.ForeignKey(User, on_delete=models.SET_NULL, null=True)
    text = models.TextField()
    created_date = models.DateTimeField(default=timezone.now)
    approved = models.BooleanField(default=True)  # Dodaj to pole

    def __str__(self):
        return f'Komentarz {self.id} od {self.author.username}'


class Match(models.Model):
    opponent_name = models.CharField(max_length=100, verbose_name="Przeciwnik")
    match_date = models.DateTimeField(verbose_name="Data meczu")
    location = models.CharField(max_length=100, verbose_name="Miejsce", default="Stadion NRG")
    base_ticket_price = models.DecimalField(max_digits=6, decimal_places=2, default=Decimal('50.00'),
                                            verbose_name="Cena bazowa biletu (normalny)")

    def __str__(self):
        return f"{self.opponent_name} - {self.match_date.strftime('%d.%m.%Y %H:%M')} (Cena: {self.base_ticket_price} PLN)"

    class Meta:
        verbose_name = "Mecz"
        verbose_name_plural = "Mecze"
        ordering = ['match_date']


class TicketType(models.Model):
    name = models.CharField(max_length=50, unique=True, verbose_name="Nazwa typu biletu")
    discount_percentage = models.DecimalField(max_digits=5, decimal_places=2, default=Decimal('0.00'),
                                              verbose_name="Zniżka (%)")

    def __str__(self): return f"{self.name} ({self.discount_percentage}%)"

    def calculate_price(self, base_price):
        if not isinstance(base_price, Decimal): base_price = Decimal(str(base_price))
        discount_amount = base_price * (self.discount_percentage / Decimal('100.00'))
        return (base_price - discount_amount).quantize(Decimal('0.01'))

    class Meta:
        verbose_name = "Typ biletu"
        verbose_name_plural = "Typy biletów"


class Ticket(models.Model):
    user = models.ForeignKey(settings.AUTH_USER_MODEL, on_delete=models.CASCADE, related_name="tickets",
                             verbose_name="Użytkownik")
    match = models.ForeignKey(Match, on_delete=models.CASCADE, related_name="tickets", verbose_name="Mecz")
    quantity = models.PositiveIntegerField(default=1, verbose_name="Ilość")
    purchase_date = models.DateTimeField(auto_now_add=True, verbose_name="Data zakupu")
    price_paid_per_ticket = models.DecimalField(max_digits=6, decimal_places=2, verbose_name="Zapłacona cena za bilet")
    total_price_paid = models.DecimalField(max_digits=8, decimal_places=2, verbose_name="Całkowita zapłacona cena")
    ticket_type = models.ForeignKey(TicketType, on_delete=models.PROTECT, verbose_name="Typ biletu")


    def save(self, *args, **kwargs):
        if self.price_paid_per_ticket is not None and self.quantity is not None:
            self.total_price_paid = self.price_paid_per_ticket * self.quantity
        else:
            self.total_price_paid = Decimal('0.00')
        super().save(*args, **kwargs)

    def __str__(self):
        user_identifier = self.user.username if self.user else "Nieznany użytkownik"
        match_identifier = self.match.opponent_name if self.match else "Nieznany mecz"
        total_price_str = f"(Łącznie: {self.total_price_paid} PLN)" if hasattr(self,
                                                                               'total_price_paid') and self.total_price_paid is not None else ""
        return f"{self.quantity}x Bilet dla {user_identifier} na {match_identifier} {total_price_str}"

    class Meta:
        verbose_name = "Bilet"
        verbose_name_plural = "Bilety"