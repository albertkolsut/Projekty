from django import forms
from django.contrib.auth.models import User
from .models import Comment, Post

class RegisterForm(forms.ModelForm):
    password = forms.CharField(widget=forms.PasswordInput)
    password_confirm = forms.CharField(widget=forms.PasswordInput, label="Confirm Password")

    class Meta:
        model = User
        fields  = ['username', 'password', 'password_confirm']
    
    def clean(self):
        cleaned_data = super().clean()
        password = cleaned_data.get('password')
        password_confirm = cleaned_data.get('password_confirm')
        if password and password_confirm and password != password_confirm:
            raise forms.ValidationError("Passwords do not match!")
        return cleaned_data


class CommentForm(forms.ModelForm):
    class Meta:
        model = Comment
        fields = ['text']
        widgets = {
            'text': forms.Textarea(attrs={
                'placeholder': 'Napisz swój komentarz...',
                'rows': 4,
                'class': 'form-control'
            }),
        }
        labels = {
            'text': ''
        }

class PostForm(forms.ModelForm):
    class Meta:
        model = Post
        fields = ['title', 'content', 'image']