import React from 'react';
import { createRoot } from 'react-dom/client';
import './styles.css';
import { AuthorApp } from './AuthorApp.jsx';
createRoot(document.getElementById('root')).render(<React.StrictMode><AuthorApp/></React.StrictMode>);
