from django.urls import path
from .views import index, bilety, aktualnosci, sklep, statystyki_zawodnikow
from django.contrib.auth.views import LoginView
from . import views

urlpatterns = [
    path('', index, name="index"),
    path('login/', views.login_view, name='login'),
    path('logout/', views.logout_view, name='logout'),
    path('register/', views.register_view, name='register'),
    path('protected/', views.ProtectedView.as_view(), name='protected'),
    path('bilety_u/', views.ProtectedViewBilety.as_view(), name='bilety_u'),
    path('aktualnosci_u/', views.ProtectedViewAktualnosci.as_view(), name='aktualnosci_u'),
    path('sklep_u/', views.ProtectedViewSklep.as_view(), name='sklep_u'),
    path('bilety/', bilety, name="bilety"),
    path('aktualnosci/', views.aktualnosci, name="aktualnosci"),
    path('sklep/', sklep, name="sklep"),
    path('statystyki/', views.statystyki_zawodnikow, name='statystyki'),
    path('tabela/', views.tabela, name='tabela'),
    path('terminarz/', views.terminarz, name='terminarz'),
    path('terminarz_u/', views.terminarz_u, name='terminarz_u'),
    path('tabela_u/', views.tabela_u, name='tabela_u'),
    path('statystyki_u/', views.statystyki_zawodnikow_u, name='statystyki_u'),
    path('ticket/return/<int:ticket_id>/', views.return_ticket_view, name='return_ticket'),
    path('merch/', views.merch_placeholder_view, name='merch'),
    path('merch_u/', views.merch_placeholder_view, name='merch_u'),
    path('<slug:slug>/', views.post_detail, name='post_detail'),
]